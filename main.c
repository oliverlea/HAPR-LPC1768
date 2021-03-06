// Created by Oliver Lea - 14/1/2014

// O-Edit: Added tests - 27/1/2014
// O-Edit: Changed interrupt handler - 8/2/2014
// O-Edit: Added Watchdog timer - 24/2/2014
// J-Edit: Refactored and tidied code - 24/2/2014
// D-Edit: Added infrared sensor work to sampling IRQ handler - 25/2/2014
// O-Edit: Fixed WDT work for the sampling IRQ handler and added a CPU
//			usage calculator - 26/2/2014
// O-Edit: Added comments describing the dacSetValue() call - 3/3/2014
// D-Edit: Fixed infrared sensor type errors - 4/3/2014

#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_wdt.h"
#include "lpc17xx_timer.h"
#include "lpc_types.h"

#include <stdlib.h>

#include "global.h"
#include "debug.h"
#include "adcInit.h"
#include "timerInit.h"
#include "watchdog.h"
#include "dacInit.h"
#include "filter.h"
#include "filterChain.h"
#include "userInterface.h"

uint16_t sampleBuffer[BUFFER_SIZE];
uint16_t *sampleP = sampleBuffer;

// Variable to hold the counter value of the WDT at the end of the sampling ISR
volatile uint32_t wdtCounter = 0;

// 0 if pass-through not selected. 1 if pass-through is selected
volatile uint8_t passThrough = 0;

// 0 if infraMix not selected. 1 if infraMix selected
volatile uint8_t infraMix = 0;

float infraValue; //infrared sensor value

uint16_t output;

// Interrupt handler that samples the ADC and sends the sample
// on to be filtered
void TIMER0_IRQHandler(void) {

	WDT_Feed();

	*sampleP = getAdcSample();

	if (passThrough) {
		output = *sampleP;
	} else if (infraMix) {

	  // infraValue will be between 0 (no hand) and 1 (hand close)
	  infraValue = (ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_2)) / 4000.0;
	  output = (((float) *sampleP) * infraValue) + (((float) applyFilters(*sampleP)) * (1.0 - infraValue));

	} else {
		output = applyFilters(*sampleP);
	}

	// Discard the least significant two bits (10bit DAC instead of 12bit ADC)
	// and output to the DAC
	dacSetValue(output>>2);

	// Select next buffer location based on previous location
	// When at the end of the buffer, loop round to the beginning next
	if (sampleP < &(sampleBuffer[BUFFER_SIZE - 1])) {
		sampleP++;
	} else {
		sampleP = sampleBuffer;
	}

	// Reset the timer (it stops when it has counted to the designated value)
	// and begin counting again
	TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);

	wdtCounter = WDT_GetCurrentCount();

	return;
}

int main(void) {

	debug_init();
	printToTerminal("UART INITIALISED\n\r");

	chain_init();
	printfToTerminal("FILTER CHAIN INITIALISED. SAMPLE RATE IS :%d\n\r", ADC_SAMPLE_RATE);

	sadc_init(ADC_SAMPLE_RATE);
	sdac_init();

	sample_timer_init(SAMPLE_RATE_US);
	watchdog_init();

	generateUI();

	return 0;
}

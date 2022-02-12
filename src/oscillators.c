/*
 * oscillators.c
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include "oscillators.h"

#include "app.h"
#include "em_cmu.h"

uint32_t oscillator_freq;

/*
 * Initializes clocks based on current energy mode setting define in app.h
 */
void init_oscillators() {

    CMU_ClockEnable(cmuClock_GPIO, true);

    // LFA CLK compatible with EM0/1/2/3, see data sheet

    // higher power energy modes - configure LFXO (32.768 KHz)
    if ((LOWEST_ENERGY_MODE == SL_POWER_MANAGER_EM0) ||
        (LOWEST_ENERGY_MODE == SL_POWER_MANAGER_EM1) ||
        (LOWEST_ENERGY_MODE == SL_POWER_MANAGER_EM2)) {
        CMU_OscillatorEnable(cmuOsc_LFXO, true, true);
        CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
    }
    // lower power energy modes - configure ULFRCO (1 kHz)
    else if (LOWEST_ENERGY_MODE == SL_POWER_MANAGER_EM3) {
        CMU_OscillatorEnable(cmuOsc_ULFRCO, true, true);
        CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_ULFRCO);
    }

    // set prescaler
    CMU_ClockDivSet(cmuClock_LETIMER0, PRESCALER);

    // turn on the clocks
    CMU_ClockEnable(cmuClock_LFA, true);
    CMU_ClockEnable(cmuClock_LETIMER0, true);

    oscillator_freq = CMU_ClockFreqGet(cmuClock_LFA); // save frequency for timer setup

}

/*
 * Allows timer setup functions to access oscillator frequency
 *
 * returns: value of global variable
 */
uint32_t get_oscillator_freq() {
    return oscillator_freq;
}

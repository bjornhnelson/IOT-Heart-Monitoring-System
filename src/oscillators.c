/*
 * oscillators.c
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include "oscillators.h"

#include "app.h"
#include "em_cmu.h"

uint32_t init_oscillators() {

  CMU_ClockEnable(cmuClock_GPIO, true);

  // LFA CLK compatible with EM0/1/2/3

  // higher power energy modes
  if ((LOWEST_ENERGY_MODE == 0) || (LOWEST_ENERGY_MODE == 1) || (LOWEST_ENERGY_MODE == 2)) {
      // configure LFXO (32.768 KHz)
      CMU_OscillatorEnable(cmuOsc_LFXO, true, true);
      CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
      // CMU_ClockDivSet() called in timers.c
  }
  // lower power energy modes
  else if (LOWEST_ENERGY_MODE == 3) {
      // configure ULFRCO (1 kHz)
      CMU_OscillatorEnable(cmuOsc_ULFRCO, true, true);
      CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_ULFRCO);
      // CMU_ClockDivSet() called in timers.c
  }

  // turn on the clocks
  CMU_ClockEnable(cmuClock_LFA, true);
  CMU_ClockEnable(cmuClock_LETIMER0, true);

  uint32_t clock_freq_hz = CMU_ClockFreqGet(cmuClock_LFA);
  return clock_freq_hz; // return frequency for timer setup

}

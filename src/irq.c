/*
 * irq.c
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include "irq.h"
#include "gpio.h"

#include "em_core.h"
#include "em_letimer.h"

void LETIMER0_IRQHandler() {

  // get enabled and pending LETIMER interrupt flags
  uint32_t flags = LETIMER_IntGetEnabled(LETIMER0);
  LETIMER_IntClear(LETIMER0, flags);

  // LED timing
  // ON from t=0 to t=175 ms
  // OFF from t=175 to t=2250 ms

  // turn on LED when CNT = 0 (underflow)
  if (flags & LETIMER_IF_UF) {
      gpioLed0SetOn();
  }
  // turn off LED when CNT = COMP1
  if (flags & LETIMER_IF_COMP1) {
      gpioLed0SetOff();
  }

}

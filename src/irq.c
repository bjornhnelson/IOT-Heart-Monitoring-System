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

  //CORE_ATOMIC_IRQ_DISABLE(); // necessary?

  // get enabled and pending LETIMER interrupt flags
  int flags = LETIMER_IntGetEnabled(LETIMER0);

  uint32_t timer_val = LETIMER_CounterGet(LETIMER0);

//  // turn off LED after 175 ms
//  if (flags & LETIMER_IF_COMP1) {
//      LETIMER_IntClear(LETIMER0, LETIMER_IFC_COMP1);
//      gpioLed0SetOff();
//
//  }
//
//  // turn on LED after 2250 ms
//  if (flags & LETIMER_IF_UF) {
//      LETIMER_IntClear(LETIMER0, LETIMER_IFC_UF);
//      gpioLed0SetOn();
//  }

  // compare 1 - turn on LED
  if (flags & LETIMER_IF_UF) {
      LETIMER_IntClear(LETIMER0, LETIMER_IFC_UF);
      gpioLed0SetOn();
  }
  // compare 2 - turn off LED
  else if (flags & LETIMER_IF_COMP1) {
      LETIMER_IntClear(LETIMER0, LETIMER_IFC_COMP1);
      gpioLed0SetOff();
  }

  //CORE_ATOMIC_IRQ_ENABLE(); // necessary?

}

/*
 * irq.c
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include "irq.h"
#include "gpio.h"
#include "scheduler.h"

#include "em_core.h"
#include "em_letimer.h"


void LETIMER0_IRQHandler() {

  CORE_ATOMIC_IRQ_DISABLE();

  // get enabled and pending LETIMER interrupt flags
  uint32_t flags = LETIMER_IntGetEnabled(LETIMER0);

  // clear the set flags
  LETIMER_IntClear(LETIMER0, flags);

  // check if timer underflow was source of interrupt
  if (flags & LETIMER_IF_UF) {

      // tell scheduler to request a temperature measurement
      scheduler_set_event(EVENT_READ_TEMP);

  }

  CORE_ATOMIC_IRQ_ENABLE();

}

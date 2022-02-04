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

  CORE_ATOMIC_IRQ_DISABLE();

  // get enabled and pending LETIMER interrupt flags
  uint32_t flags = LETIMER_IntGetEnabled(LETIMER0);
  LETIMER_IntClear(LETIMER0, flags);

  if (flags & LETIMER_IF_UF) {
      // measure temperature
      schedulerSetEventUF();
  }

  CORE_ATOMIC_IRQ_ENABLE();

}

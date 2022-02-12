/*
 * irq.c
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include "irq.h"
#include "gpio.h"
#include "scheduler.h"
#include "app.h"
#include "timers.h"
#include "i2c.h"

#include "em_core.h"
#include "em_letimer.h"
#include "em_i2c.h"

// global variables
uint32_t num_underflows = 0;


void I2C0_IRQHandler() {
    CORE_ATOMIC_IRQ_DISABLE();

    I2C_TransferReturn_TypeDef transfer_status = I2C_Transfer(I2C0);

    if (transfer_status == i2cTransferDone) {
        scheduler_set_event(EVENT_I2C_DONE);
    }

    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }

    CORE_ATOMIC_IRQ_ENABLE();
}

void LETIMER0_IRQHandler() {

  CORE_ATOMIC_IRQ_DISABLE();

  // get enabled and pending LETIMER interrupt flags
  uint32_t flags = LETIMER_IntGetEnabled(LETIMER0);

  // clear the set flags
  LETIMER_IntClear(LETIMER0, flags);

  // check if timer underflow was source of interrupt
  if (flags & LETIMER_IF_UF) {

      // update timestamps in logging
      count_underflows();

      // tell scheduler to request a temperature measurement
      scheduler_set_event(EVENT_MEASURE_TEMP);

  }

  // check if CNT == COMP1 was source of interrupt
  if (flags & LETIMER_IF_COMP1) {
      LETIMER_IntDisable(LETIMER0, LETIMER_IEN_COMP1);
      scheduler_set_event(EVENT_TIMER_EXPIRED);
  }

  CORE_ATOMIC_IRQ_ENABLE();

}

// counts number of times underflow interrupt occurred
void count_underflows() {
    num_underflows++;
}

uint32_t letimerMilliseconds() {

    // map freq to period
    /*uint16_t ticks_per_ms = timer_max_ticks / LETIMER_PERIOD_MS;

    uint32_t cur_period_ms = (timer_max_ticks - LETIMER_CounterGet(LETIMER0)) / ticks_per_ms;*/

    uint32_t result = num_underflows * LETIMER_PERIOD_MS;
    //uint32_t result = (num_underflows * LETIMER_PERIOD_MS) + cur_period_ms;

    return result;
}

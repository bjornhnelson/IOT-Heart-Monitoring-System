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

// enable logging
#define INCLUDE_LOG_DEBUG 1
#include "log.h"

// global variables
uint32_t num_underflows = 0;


// I2C interrupt service routine
void I2C0_IRQHandler() {
    I2C_TransferReturn_TypeDef transfer_status = I2C_Transfer(I2C0);

    if (transfer_status == i2cTransferDone) {
        scheduler_set_event_I2C();
    }

    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }

}

// LE Timer interrupt service routine
void LETIMER0_IRQHandler() {

  // get enabled and pending LETIMER interrupt flags
  uint32_t flags = LETIMER_IntGetEnabled(LETIMER0);

  // clear the set flags
  LETIMER_IntClear(LETIMER0, flags);

  // check if timer underflow was source of interrupt
  if (flags & LETIMER_IF_UF) {

      // update timestamps in logging
      count_underflows();

      // tell scheduler to request a temperature measurement
      scheduler_set_event_UF();

  }

  // check if CNT == COMP1 was source of interrupt
  if (flags & LETIMER_IF_COMP1) {
      LETIMER_IntDisable(LETIMER0, LETIMER_IEN_COMP1);
      scheduler_set_event_COMP1();
  }

}

// counts number of times underflow interrupt occurred
void count_underflows() {
    num_underflows++;
}

/*
 * Calculates the amount of time since system startup
 *
 * returns: time value in milliseconds
 */
uint32_t letimerMilliseconds() {

    uint32_t timer_cur_ticks = LETIMER_CounterGet(LETIMER0);
    uint32_t timer_elapsed_ticks = timer_max_ticks - timer_cur_ticks;
    uint32_t timer_elapsed_ms = timer_elapsed_ticks * LETIMER_PERIOD_MS / timer_max_ticks;

    uint32_t result = (num_underflows * LETIMER_PERIOD_MS) + timer_elapsed_ms;

    return result;
}

/*
 * timers.c
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include "timers.h"
#include "oscillators.h"
#include "app.h"

#include "em_cmu.h"
#include "em_letimer.h"

#include "stdint.h"

#define USEC_PER_MSEC 1000
#define MSEC_PER_SEC 1000
#define USEC_TO_SEC 1000000

// global variables
uint32_t clock_freq_hz; // frequency / prescaler
uint16_t cnt_max; // saving COMP 0 register for use in wait function

// data structure for initializing LETIMER0
// https://siliconlabs.github.io/Gecko_SDK_Doc/efm32g/html/structLETIMER__Init__TypeDef.html
static const LETIMER_Init_TypeDef letimer_settings =
{
    .enable = false, // Start counting when initialization completes
    .debugRun = true, // Counter shall keep running during debug halt
    .comp0Top = true, // Load COMP0 register into CNT when counter underflows
    .bufTop = false, // Load COMP1 into COMP0 when REP0 reaches 0
    .out0Pol = 0, // Idle value for output 0
    .out1Pol = 0, // Idle value for output 1
    .ufoa0 = letimerUFOANone, // Underflow output 0 action
    .ufoa1 = letimerUFOANone, // Underflow output 1 action
    .repMode = letimerRepeatFree, // Repeat mode
    .topValue = 0 // Top value. Counter wraps when top value matches counter value is reached

};


/*
 * initializes LETIMER0
 *
 * clock_freq = LFA freq determined in oscillator initialization
 */
void init_timer(uint32_t clock_freq) {

    // save into global variable
    clock_freq_hz = clock_freq / PRESCALER;

    // initialize with settings in structure
    LETIMER_Init(LETIMER0, &letimer_settings);

    // make sure command register is not busy
    while (LETIMER0->SYNCBUSY != 0);

    // counter value for entire period, 3000 ms
    // num ticks = (clock frequency / prescaler) * desired time duration
    uint32_t comp0_value = clock_freq_hz * LETIMER_PERIOD_MS / MSEC_PER_SEC;
    cnt_max = comp0_value;

    // set compare registers for timer
    LETIMER_CompareSet(LETIMER0, 0, comp0_value);

    // enable interrupts for doing si7021 measurements
    LETIMER_IntEnable(LETIMER0, LETIMER_IEN_UF);

    // start running the timer
    LETIMER_Enable(LETIMER0, true);

}

/*
 * Delays for a specified number of microseconds
 * Maximum supported delay = 3 seconds
 *
 * us_wait = delay duration in us
 */
void timerWaitUs(uint32_t us_wait) {

      uint16_t ms_wait = us_wait / USEC_PER_MSEC;

      // range checking
      if (ms_wait >= LETIMER_PERIOD_MS) {
          ms_wait = LETIMER_PERIOD_MS - 1;
          //LOG_WARN("Requested delay exceeds supported maximum. Delay time capped to %d msecs", LETIMER_PERIOD_MS);
      }

      // compute how many ticks for specified delay
      uint16_t delay_ticks = clock_freq_hz * ms_wait / MSEC_PER_SEC;
      uint16_t start_tick = LETIMER_CounterGet(LETIMER0);

      uint16_t stop_tick = 0;

      // case 1: normal counting
      if (delay_ticks < start_tick) {
          stop_tick = start_tick - delay_ticks;
      }
      // case 2: wraparound from 0 -> COMP0
      else {
          stop_tick = cnt_max - (delay_ticks - start_tick);
      }

      // do nothing until reaching the correct stop tick
      while (LETIMER_CounterGet(LETIMER0) != stop_tick);

}

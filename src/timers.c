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
#include "src/gpio.h"


#define USEC_PER_MSEC 1000
#define MSEC_PER_SEC 1000

uint32_t clock_freq_hz = 0;

int ledStatus = 0;

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


// freq_hz: 1 kHz or 32.768 KHz (LFA frequency)
void init_timer(uint32_t clock_freq) {

    // set global variable
    clock_freq_hz = clock_freq / PRESCALER;

    // initialize with settings in structure
    LETIMER_Init(LETIMER0, &letimer_settings);

    // make sure command register is not busy
    while (LETIMER0->SYNCBUSY != 0);

    // num ticks = (clock frequency / prescaler) * desired time duration

    // counter value for entire period, 3000 ms
    uint32_t comp0_value = clock_freq_hz * LETIMER_PERIOD_MS / MSEC_PER_SEC;

    // set compare registers for timer
    LETIMER_CompareSet(LETIMER0, 0, comp0_value);

    // enable interrupts for doing si7021 measurements
    //LETIMER_IntEnable(LETIMER0, LETIMER_IEN_UF);

    // start running the timer
    LETIMER_Enable(LETIMER0, true);

}

void timerWaitUs(uint32_t us_wait) {

    // 8 sec max delay for 32.768 kHz clock, prescaler of 4
    uint16_t ms_max_delay = UINT16_MAX * MSEC_PER_SEC / clock_freq_hz;

    uint16_t ms_wait = us_wait / USEC_PER_MSEC;

    if (ms_wait > ms_max_delay) {
        ms_wait = ms_max_delay;
        //LOG_WARN("Requested delay exceeds supported maximum. Delay time capped to %lu usecs", ms_wait * USEC_PER_MSEC);
    }

    uint16_t delay_ticks = clock_freq_hz * ms_wait / MSEC_PER_SEC;

    uint16_t start_tick = LETIMER_CounterGet(LETIMER0);


    // great explanation on handling the wraparound case
    // https://arduino.stackexchange.com/questions/12587/how-can-i-handle-the-millis-rollover
    uint16_t cur_tick;
    while(1) {
        cur_tick = LETIMER_CounterGet(LETIMER0);
        if ((uint16_t)(start_tick - cur_tick) > delay_ticks) {
            break;
        }
    }
    //while (LETIMER_CounterGet(LETIMER0) - start_tick < delay_ticks);

    //int rollover_case = (delay_ticks > start_tick) ? true : false;

    // will wrap around from 0 -> 65535 (2^16 - 1)
    //uint16_t stop_tick = start_tick - delay_ticks;

    // extra delay to hit CNT = 0
    /*if (rollover_case) {
        gpioLed1SetOn();
        while (true) {
            if (LETIMER_CounterGet(LETIMER0) > start_tick) {
                break;
            }
        }
    }*/

    // check duration, don't compare timestamps
    /*uint16_t cur_tick;
    while (1) {
        cur_tick = LETIMER_CounterGet(LETIMER0);
        if (cur_tick - start_tick > delay_ticks) {
            break;
        }
    }*/

    /*while (true) {
        if (stop_tick >= LETIMER_CounterGet(LETIMER0)) {
            break;
        }
    }*/
    //gpioLed1SetOff();

    // LOG_DEBUG("exiting");

}

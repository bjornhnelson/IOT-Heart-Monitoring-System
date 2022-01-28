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

#define MSEC_PER_SEC 1000

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


// clock_freq_hz: 1 kHz or 32.768 KHz (LFA frequency)
void init_timer(uint32_t clock_freq_hz) {

    // initialize with settings in structure
    LETIMER_Init(LETIMER0, &letimer_settings);

    // make sure command register is not busy
    while (LETIMER0->SYNCBUSY != 0);

    // counter value for entire period, 2250 ms
    uint32_t comp0_value = clock_freq_hz * LETIMER_PERIOD_MS / (MSEC_PER_SEC * PRESCALER);

    // counter value for when LED is off, 2075 ms
    uint32_t comp1_value = clock_freq_hz * LETIMER_OFF_TIME_MS / (MSEC_PER_SEC * PRESCALER);

    // set compare registers for timer
    LETIMER_CompareSet(LETIMER0, 0, comp0_value);
    LETIMER_CompareSet(LETIMER0, 1, comp1_value);

    // enable interrupts for toggling LED
    LETIMER_IntEnable(LETIMER0, LETIMER_IEN_COMP1 | LETIMER_IEN_UF);

    // start running the timer
    LETIMER_Enable(LETIMER0, true);

}

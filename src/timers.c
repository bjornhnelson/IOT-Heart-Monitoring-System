/*
 * timers.c
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include "timers.h"
#include "app.h"

#include "em_cmu.h"
#include "em_letimer.h"

#define MSEC_PER_SEC 1000
#define PRESCALER 4

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


// clock_freq_hz: LFA frequency - 1 kHz or 32.768 KHz
void init_timer(uint32_t clock_freq_hz) {
    // start with LED off
    LETIMER_IntClear(LETIMER0, LETIMER_IFC_COMP1);

    // initialize with settings in typedef
    LETIMER_Init(LETIMER0, &letimer_settings);

    // make sure command register is not busy
    while(LETIMER0->SYNCBUSY != 0);

    LETIMER0->IFC &= 0;

    uint32_t comp0_value = 0;
    uint32_t comp1_value = 0;

    // max value of count
    comp0_value = clock_freq_hz * LETIMER_PERIOD_MS / (MSEC_PER_SEC * PRESCALER);

    // intermediate value of count
    comp1_value = clock_freq_hz * LETIMER_OFF_TIME_MS / (MSEC_PER_SEC * PRESCALER);

    // set prescaler
    CMU_ClockDivSet(cmuClock_LETIMER0, PRESCALER);

    // set timer period for turning on the LED
    LETIMER_CompareSet(LETIMER0, 0, comp0_value);

    // set timer period for turning off the LED
    LETIMER_CompareSet(LETIMER0, 1, comp1_value);

    // enable interrupts
    LETIMER_IntEnable(LETIMER0, LETIMER_IEN_COMP1 | LETIMER_IEN_UF);

    LETIMER_Enable(LETIMER0, true);

}

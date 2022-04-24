/*
 * led.c
 *
 *  Created on: Apr 23, 2022
 *      Author: bjornnelson
 */

#include "stdbool.h"
#include "ble.h"
#include "irq.h"
#include "limits.h"
#include "led.h"
#include "gpio.h"

#define ON 1
#define OFF 0

bool ledStatus = OFF;

// set to an actual value in scheduler.c
uint64_t next_pulse_time = ULLONG_MAX;

uint16_t get_LED_period(uint16_t bpm) {
    uint16_t result = SEC_PER_MIN * MSEC_PER_SEC / bpm;
    result /= 2;
    return result;
}


void pulse_LED() {

    if (letimerMilliseconds() > next_pulse_time) {

        next_pulse_time = letimerMilliseconds() + get_LED_period(get_ble_data_ptr()->heart_rate);

        if (ledStatus == ON) {
            ledStatus = OFF;
            gpioLed0SetOff();
        }
        else {
            ledStatus = ON;
            gpioLed0SetOn();
        }
    }

}

/*
 * led.h
 *
 *  Created on: Apr 23, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_LED_H_
#define SRC_LED_H_

#include "stdint.h"

#define SEC_PER_MIN 60
#define MSEC_PER_SEC 1000

extern uint64_t next_pulse_time;

uint16_t get_LED_period(uint16_t bpm);
void pulse_LED();

#endif /* SRC_LED_H_ */

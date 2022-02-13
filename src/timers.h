/*
 * timers.h
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include <stdint.h>

#ifndef SRC_TIMERS_H_
#define SRC_TIMERS_H_


extern uint32_t clock_freq_hz;
extern uint16_t timer_max_ticks;

void init_timer();
void timer_wait_us_polled(uint32_t us_wait);
void timer_wait_us_IRQ(uint32_t us_wait);

#endif /* SRC_TIMERS_H_ */

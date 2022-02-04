/*
 * timers.h
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include <stdint.h>

#ifndef SRC_TIMERS_H_
#define SRC_TIMERS_H_

void init_timer(uint32_t clock_freq);
void timerWaitUs(uint32_t us_wait);

#endif /* SRC_TIMERS_H_ */

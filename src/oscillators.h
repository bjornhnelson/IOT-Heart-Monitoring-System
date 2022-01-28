/*
 * oscillators.h
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#include <stdint.h>

#ifndef SRC_OSCILLATORS_H_
#define SRC_OSCILLATORS_H_

#define PRESCALER 4 // referenced in timers.c

uint32_t init_oscillators();

#endif /* SRC_OSCILLATORS_H_ */

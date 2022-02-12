/*
 * scheduler.h
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_SCHEDULER_H_
#define SRC_SCHEDULER_H_

#include "stdint.h"

typedef enum {
    EVENT_IDLE,
    EVENT_MEASURE_TEMP,
    EVENT_TIMER_EXPIRED,
    EVENT_I2C_DONE,
    EVENT_I2C_ERROR,
} events_t;

typedef enum {
    STATE_IDLE,
    STATE_SENSOR_POWERUP,
    STATE_I2C_WRITE,
    STATE_INTERIM_DELAY,
    STATE_I2C_READ,
} states_t;

void init_scheduler();

void scheduler_set_event(uint8_t event);

void scheduler_clear_event();

uint8_t scheduler_get_next_event();

void scheduler_state_machine();

#endif /* SRC_SCHEDULER_H_ */

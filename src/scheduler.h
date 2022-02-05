/*
 * scheduler.h
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_SCHEDULER_H_
#define SRC_SCHEDULER_H_

#include "stdint.h"

// represents no events pending
#define IDLE (0)

// bit 0 assigned to temperature read event
// bits 1-7 currently unassigned
#define EVENT_READ_TEMP (1 << 0)

// number of event statuses that can be handled simultaneously
#define MAX_EVENTS 8

// data structure to monitor status of up to  8 events
// bit toggled to 1 -> event occurred
static volatile uint8_t event_flags;

void init_scheduler();

void scheduler_set_event(uint8_t event);

void scheduler_clear_event(uint8_t event);

uint8_t get_next_event();

#endif /* SRC_SCHEDULER_H_ */

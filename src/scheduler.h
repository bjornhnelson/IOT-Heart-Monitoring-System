/*
 * scheduler.h
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_SCHEDULER_H_
#define SRC_SCHEDULER_H_

#include "stdint.h"

#define IDLE (0)

#define EVENT_READ_TEMP (1 << 0)
#define MAX_EVENTS 8

// data structure to monitor status of up to  8 events
// bit toggled to 1 -> event occurred
static volatile uint8_t event_flags;

// scheduler routine to initialize the data structure
void init_scheduler();

// scheduler routine to set a scheduler event
void scheduler_set_event(uint8_t event);

// scheduler routine to clear a scheduler event
void scheduler_clear_event(uint8_t event);

// scheduler routine to return 1 event to main()code and clear that event
uint8_t get_next_event();

#endif /* SRC_SCHEDULER_H_ */

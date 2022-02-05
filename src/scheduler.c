/*
 * scheduler.c
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#include "scheduler.h"

#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

void init_scheduler() {
    event_flags = IDLE;
    LOG_INFO("Scheduler started");
}

// scheduler routine to set a scheduler event
void scheduler_set_event(uint8_t event) {
    CORE_ATOMIC_IRQ_DISABLE();
    event_flags |= event;
    CORE_ATOMIC_IRQ_ENABLE();
}

// scheduler routine to clear a scheduler event
void scheduler_clear_event(uint8_t event) {
    CORE_ATOMIC_IRQ_DISABLE();
    event_flags &= ~event;
    CORE_ATOMIC_IRQ_ENABLE();
}

// scheduler routine to return 1 event to main()code and clear that event
uint8_t get_next_event() {
    if (event_flags == IDLE)
        return IDLE;

    uint8_t result_event;

    // Event 0 highest priority, event 7 lowest priority
    for (int i=0; i<MAX_EVENTS; i++) {
        if ((1 << i) & event_flags) {
            result_event = (1 << i);
            break;
        }
    }

    // clear the event
    scheduler_clear_event(result_event);

    return result_event;
}


/*
 * scheduler.c
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#include "scheduler.h"

//#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

// resets the event data structure at initialization
void init_scheduler() {
    event_flags = IDLE;
    LOG_INFO("Scheduler started");
}

/*
 * routine to set a scheduler event
 *
 * event = bit shifted event index
 * (1 << index)
 */
void scheduler_set_event(uint8_t event) {
    CORE_ATOMIC_IRQ_DISABLE();
    event_flags |= event;
    CORE_ATOMIC_IRQ_ENABLE();
}

/*
 * routine to clear a scheduler event
 *
 * event = bit shifted event index
 * (1 << index)
 */
void scheduler_clear_event(uint8_t event) {
    CORE_ATOMIC_IRQ_DISABLE();
    event_flags &= ~event;
    CORE_ATOMIC_IRQ_ENABLE();
}

/*
 * routine to return 1 single event to main() code and clear that event
 *
 * returns: bit shifted event index
 */
uint8_t get_next_event() {

    // no event currently happening
    if (event_flags == IDLE)
        return IDLE;

    uint8_t result_event;

    // support for simultaneous events
    // event 0 has highest priority, event 7 has lowest priority
    for (int i=0; i<MAX_EVENTS; i++) {
        if ((1 << i) & event_flags) {
            result_event = (1 << i);
            break;
        }
    }

    // clear the event
    scheduler_clear_event(result_event);

    // 1 << event_index
    return result_event;
}


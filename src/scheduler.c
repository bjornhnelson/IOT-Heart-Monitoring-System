/*
 * scheduler.c
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#include "scheduler.h"

#include "i2c.h"
#include "gpio.h"
#include "timers.h"

#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

static events_t cur_event;
static states_t cur_state;

// resets the event data structure at initialization
void init_scheduler() {
    cur_state = STATE_IDLE;
    cur_event = EVENT_IDLE;
    LOG_INFO("Scheduler started");
}

// routine to set a scheduler event, used by IRQs
void scheduler_set_event(uint8_t event) {
    CORE_ATOMIC_IRQ_DISABLE();
    cur_event = event;
    CORE_ATOMIC_IRQ_ENABLE();
}

// DELETE??
// routine to clear a scheduler event
void scheduler_clear_event() {
    CORE_ATOMIC_IRQ_DISABLE();
    cur_event = EVENT_IDLE;
    CORE_ATOMIC_IRQ_ENABLE();
}

// DELETE??
uint8_t scheduler_get_event() {
    return cur_event;
}

void scheduler_state_machine() {

    uint8_t next_state = cur_state;

    switch (cur_state) {

        // comment
        case STATE_IDLE:
            if (cur_event == EVENT_MEASURE_TEMP) {
                next_state = STATE_SENSOR_POWERUP;

                // initialize temp sensor
                gpioSi7021Enable();

                // wait 80 ms for sensor to power up
                timerWaitUsIRQ(80000);

            }
        break;

        // comment
        case STATE_SENSOR_POWERUP:
            if (cur_event == EVENT_TIMER_EXPIRED) {
                next_state = STATE_I2C_WRITE;

                // power manager on

                // send the command
                i2c_send_command();

            }
        break;

        // comment
        case STATE_I2C_WRITE:
            if (cur_event == EVENT_I2C_DONE) {
                next_state = STATE_INTERIM_DELAY;

                // power manager off

                // disable i2c interrupts
                NVIC_DisableIRQ(I2C0_IRQn);

                // 10 ms delay
                timerWaitUsIRQ(10000);
            }
        break;

        // comment
        case STATE_INTERIM_DELAY:
            if (cur_event == EVENT_TIMER_EXPIRED) {
                next_state = STATE_I2C_READ;

                // power manager on
                i2c_receive_data();

            }
        break;

        // comment
        case STATE_I2C_READ:
            if (cur_event == EVENT_I2C_DONE) {
                next_state = STATE_IDLE;

                // power manager off

                // disable i2c interrupts
                NVIC_DisableIRQ(I2C0_IRQn);\

                // print the value
                print_temperature();

                // do all deinitialization
                deinit_i2c();

            }
        break;
    }

    if (cur_state != next_state) {
        LOG_INFO("STATE TRANSITION: %d -> %d", cur_state, next_state);
        cur_state = next_state;
    }
}



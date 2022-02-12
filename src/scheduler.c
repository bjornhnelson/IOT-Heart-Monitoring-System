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

// status variable for most recent event that occurred
// events are set by the IRQs
static events_t cur_event;

// status variable for the current state of the system
static states_t cur_state;

// resets the data structures at initialization
void init_scheduler() {
    cur_state = STATE_IDLE;
    cur_event = EVENT_IDLE;
    //LOG_INFO("Scheduler started");
}

/*
 * routine to set a scheduler event, called by IRQs
 *
 * event = value to set global status variable
 */
void scheduler_set_event(uint8_t event) {
    CORE_ATOMIC_IRQ_DISABLE();
    cur_event = event;
    CORE_ATOMIC_IRQ_ENABLE();
}

// routine to clear a scheduler event, not currently used
void scheduler_clear_event() {
    CORE_ATOMIC_IRQ_DISABLE();
    cur_event = EVENT_IDLE;
    CORE_ATOMIC_IRQ_ENABLE();
}

// processing logic for handling states and events
void scheduler_state_machine() {

    // initially assume system will remain in current state
    uint8_t next_state = cur_state;

    switch (cur_state) {

        case STATE_IDLE:

            // when underflow interrupt occurs
            if (cur_event == EVENT_MEASURE_TEMP) {
                next_state = STATE_SENSOR_POWERUP;

                // initialize i2c
                init_i2c();

                // initialize temp sensor
                gpioSi7021Enable();

                // wait 80 ms for sensor to power up
                timer_wait_us_IRQ(80000);
            }
        break;

        case STATE_SENSOR_POWERUP:

            // when COMP 1 interrupt occurs
            if (cur_event == EVENT_TIMER_EXPIRED) {
                next_state = STATE_I2C_WRITE;

                // EM <= 1 required during I2C transfers
                sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

                // send the request for a temperature measurement
                i2c_send_command();
            }
        break;

        case STATE_I2C_WRITE:

            // when I2C interrupt occurs
            if (cur_event == EVENT_I2C_DONE) {
                next_state = STATE_INTERIM_DELAY;

                // I2C transfer over, EM <= 1 no longer required
                sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);

                // disable i2c interrupts
                NVIC_DisableIRQ(I2C0_IRQn);

                // 10 ms delay between write and read
                timer_wait_us_IRQ(10000);
            }
        break;

        case STATE_INTERIM_DELAY:

            // when COMP1 interrupt occurs
            if (cur_event == EVENT_TIMER_EXPIRED) {
                next_state = STATE_I2C_READ;

                // EM <= 1 required during I2C transfers
                sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

                // save the temperature measurement in i2c.c variable
                i2c_receive_data();
            }
        break;

        case STATE_I2C_READ:

            // when i2c interrupt occurs
            if (cur_event == EVENT_I2C_DONE) {
                next_state = STATE_IDLE;

                // I2C transfer over, EM <= 1 no longer required
                sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);

                // disable i2c interrupts
                NVIC_DisableIRQ(I2C0_IRQn);

                // log the temperature value
                print_temperature();

                // deinitialize i2c
                deinit_i2c();
            }
        break;
    }

    // do a state transition
    if (cur_state != next_state) {

        /*
         * IMPORTANT NOTE:
         * Leave this log statement commented out unless the board is running in EM0
         * Logging is slow and will cause the i2c interrupts to be missed
         */
        //LOG_INFO("STATE TRANSITION: %d -> %d", cur_state, next_state);

        // update global status variable
        cur_state = next_state;
    }
}

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
#include "ble.h"
#include "lcd.h"
#include "heart_sensor.h"
#include "irq.h"
#include "led.h"

#include "em_letimer.h"

//#define INCLUDE_LOG_DEBUG 1
#include "log.h"

// status variables for the current state of the system
static server_states_t cur_server_state;

static heart_states_t cur_state;

// resets the data structures at initialization
void init_scheduler() {
    cur_server_state = STATE_IDLE;
    cur_state = STATE_WAITING;
    //LOG_INFO("Scheduler started");
}

// signals to bluetooth stack that external event occurred (3 second timer elapsed)
void scheduler_set_event_UF() {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    //sl_bt_external_signal(EVENT_MEASURE_TEMP);
    sl_bt_external_signal(EVENT_CHECK_SENSOR);
    CORE_EXIT_CRITICAL();
}

// signals to bluetooth stack that external event occurred (custom timer finished)
void scheduler_set_event_COMP1() {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    sl_bt_external_signal(EVENT_TIMER_EXPIRED);
    CORE_EXIT_CRITICAL();
}

// signals to bluetooth stack that external event occurred (i2c operation finished)
void scheduler_set_event_I2C() {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    sl_bt_external_signal(EVENT_I2C_DONE);
    CORE_EXIT_CRITICAL();
}

// signals to bluetooth stack that external event occurred (button 0 pressed)
void scheduler_set_event_PB0_pressed() {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    get_ble_data_ptr()->pb0Pressed = true;
    sl_bt_external_signal(EVENT_PB0);
    CORE_EXIT_CRITICAL();
}

// signals to bluetooth stack that external event occurred (button 0 released)
void scheduler_set_event_PB0_released() {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    get_ble_data_ptr()->pb0Pressed = false;
    sl_bt_external_signal(EVENT_PB0); // trigger on rising and falling edge
    CORE_EXIT_CRITICAL();
}

// signals to bluetooth stack that external event occurred (button 1 pressed)
void scheduler_set_event_PB1_pressed() {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    get_ble_data_ptr()->pb1Pressed = true;
    sl_bt_external_signal(EVENT_PB1); // trigger on rising and falling edge
    CORE_EXIT_CRITICAL();
}

// signals to bluetooth stack that external event occurred (button 1 released)
void scheduler_set_event_PB1_released() {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    get_ble_data_ptr()->pb1Pressed = false;
    //sl_bt_external_signal(EVENT_PB1); // don't trigger on falling edge
    CORE_EXIT_CRITICAL();
}

/*
 * check if bluetooth connection closed or indications not enabled
 * returns: boolean indicating whether to go to idle state
 */
/*uint8_t bluetooth_connection_errors() {
    return (!(get_ble_data_ptr()->connectionOpen) || !(get_ble_data_ptr()->tempIndicationsEnabled));
}*/

/*
 * helper function for state machine
 * 1. checks if event is an external signal
 * 2. if so, checks if extsignals matches event_id input parameter
 *
 * evt = event passed into state machine
 * event_id = desired event to compare against
 *
 * returns: true if index is correct and the event matches, false if not
 */
uint8_t external_signal_event_match(sl_bt_msg_t* evt, uint8_t event_id) {
    if (SL_BT_MSG_ID(evt->header) != sl_bt_evt_system_external_signal_id) {
        return false;
    }

    return (evt->data.evt_system_external_signal.extsignals == event_id);

}

void heart_sensor_state_machine(sl_bt_msg_t* evt) {

    // initially assume system will remain in current state
    uint8_t next_state = cur_state;

    switch (cur_state) {

        case STATE_WAITING:
            //LOG_INFO("State Waiting for Readings");

            if (external_signal_event_match(evt, EVENT_CHECK_SENSOR)) {

                read_heart_sensor();

                // object detected or finger detected
                if ((get_heart_data_ptr()->finger_status == OBJECT_DETECTED) || get_heart_data_ptr()->finger_status == FINGER_DETECTED) {
                    LOG_INFO("State transition: waiting -> acquiring data");
                    displayPrintf(DISPLAY_ROW_ACTION, "Acquiring data...");
                    next_state = STATE_ACQUIRING_DATA;

                    timer_wait_us_IRQ(1000000);
                }

            }
            break;

        case STATE_ACQUIRING_DATA:
            //LOG_INFO("State Acquiring Readings");

            if (external_signal_event_match(evt, EVENT_TIMER_EXPIRED)) {

                read_heart_sensor();

                if ((get_heart_data_ptr()->finger_status == NOTHING_DETECTED)) {
                    displayPrintf(DISPLAY_ROW_ACTION, "Place Finger!");
                    next_state = STATE_WAITING;
                    LOG_INFO("State transition: acquiring data -> waiting");
                }
                else if ((get_heart_data_ptr()->confidence > CONFIDENCE_THRESHOLD) && (get_heart_data_ptr()->finger_status == FINGER_DETECTED)) {
                    next_state = STATE_RETURNING_DATA;
                    LOG_INFO("State transition: acquiring data -> returning data");
                }

                timer_wait_us_IRQ(1000000);
            }

            break;

        case STATE_RETURNING_DATA:
            //LOG_INFO("State Returning Readings");

            if (external_signal_event_match(evt, EVENT_TIMER_EXPIRED)) {
                read_heart_sensor();

                if ((get_heart_data_ptr()->finger_status == NOTHING_DETECTED)) {
                    displayPrintf(DISPLAY_ROW_ACTION, "Place Finger!");
                    next_state = STATE_WAITING;
                    LOG_INFO("State transition: returning data -> waiting");
                }
                else if (get_heart_data_ptr()->confidence < CONFIDENCE_THRESHOLD) {
                    displayPrintf(DISPLAY_ROW_ACTION, "Acquiring data...");
                    next_state = STATE_ACQUIRING_DATA;
                    LOG_INFO("State transition: returning data -> acquiring data");
                }
                else {
                    get_ble_data_ptr()->heart_rate = get_heart_data_ptr()->heart_rate;
                    get_ble_data_ptr()->blood_oxygen = get_heart_data_ptr()->blood_oxygen;
                    get_ble_data_ptr()->confidence = get_heart_data_ptr()->confidence;

                    displayPrintf(DISPLAY_ROW_ACTION, "");
                    displayPrintf(DISPLAY_ROW_8, "Heart Rate: %d BPM", get_ble_data_ptr()->heart_rate);
                    displayPrintf(DISPLAY_ROW_9, "Blood Oxygen: %d%%", get_ble_data_ptr()->blood_oxygen);
                    displayPrintf(DISPLAY_ROW_10, "Confidence: %d%%", get_ble_data_ptr()->confidence);

                    /*LOG_INFO("** Heart Rate: %d", get_ble_data_ptr()->heart_rate);
                    LOG_INFO("** Blood Oxygen: %d", get_ble_data_ptr()->blood_oxygen);*/

                    ble_transmit_heart_data();

                    // update variable for LED pulsing
                    next_pulse_time = letimerMilliseconds() + get_LED_period(get_ble_data_ptr()->heart_rate);

                }

                timer_wait_us_IRQ(1000000);
            }

            break;
    }

    if (cur_state != next_state) {
        cur_state = next_state; // update global status variable
    }

}

// server processing logic for handling states and events
void temperature_state_machine(sl_bt_msg_t* evt) {

    // initially assume system will remain in current state
    uint8_t next_state = cur_server_state;

    switch (cur_server_state) {

        case STATE_IDLE:

            // return to idle case
            if (bluetooth_connection_errors()) {
                next_state = STATE_IDLE;
            }

            // when underflow interrupt occurs
            else if (external_signal_event_match(evt, EVENT_MEASURE_TEMP)) {
                next_state = STATE_SENSOR_POWERUP;

                // initialize i2c
                init_i2c();

                // wait 80 ms for sensor to power up
                timer_wait_us_IRQ(80000);
            }
            break;

        case STATE_SENSOR_POWERUP:

            // return to idle case
            if (bluetooth_connection_errors()) {
                next_state = STATE_IDLE;
                LETIMER_IntDisable(LETIMER0, LETIMER_IEN_COMP1); // disable comp1 interrupt
                deinit_i2c();
            }

            // when COMP 1 interrupt occurs
            else if (external_signal_event_match(evt, EVENT_TIMER_EXPIRED)) {
                next_state = STATE_I2C_WRITE;

                // EM <= 1 required during I2C transfers
                sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

                // send the request for a temperature measurement
                i2c_send_command();
            }
            break;

        case STATE_I2C_WRITE:

            // return to idle case
            if (bluetooth_connection_errors()) {
                next_state = STATE_IDLE;
                NVIC_DisableIRQ(I2C0_IRQn);
                deinit_i2c();
                sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
            }

            // when I2C interrupt occurs
            else if (external_signal_event_match(evt, EVENT_I2C_DONE)) {
                next_state = STATE_INTERIM_DELAY;

                // I2C transfer over, EM <= 1 no longer required
                sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);

                // disable i2c interrupts
                NVIC_DisableIRQ(I2C0_IRQn);

                // 10.8 ms delay between write and read
                timer_wait_us_IRQ(10800);
            }
            break;

        case STATE_INTERIM_DELAY:

            // return to idle case
            if (bluetooth_connection_errors()) {
                next_state = STATE_IDLE;
                LETIMER_IntDisable(LETIMER0, LETIMER_IEN_COMP1); // disable comp1 interrupt
                deinit_i2c();
            }

            // when COMP1 interrupt occurs
            else if (external_signal_event_match(evt, EVENT_TIMER_EXPIRED)) {
                next_state = STATE_I2C_READ;

                // EM <= 1 required during I2C transfers
                sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

                // saves the temperature measurement in i2c.c variable
                i2c_receive_data();
            }
            break;

        case STATE_I2C_READ:

            // return to idle case
            if (bluetooth_connection_errors()) {
                next_state = STATE_IDLE;
                NVIC_DisableIRQ(I2C0_IRQn);
                deinit_i2c();
                sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
            }

            // when i2c interrupt occurs
            else if (external_signal_event_match(evt, EVENT_I2C_DONE)) {
                next_state = STATE_IDLE;

                // I2C transfer over, EM <= 1 no longer required
                sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);

                // disable i2c interrupts
                NVIC_DisableIRQ(I2C0_IRQn);

                // send the temperature value
                ble_transmit_temp();

                // deinitialize i2c
                deinit_i2c();
            }
            break;
    }

    // do a state transition
    if (cur_server_state != next_state) {
        cur_server_state = next_state; // update global status variable

        /*
         * IMPORTANT NOTE:
         * Leave this log statement commented out unless the board is running in EM0
         * Logging is slow and will cause the i2c interrupts to be missed
         */
        //LOG_INFO("STATE TRANSITION: %d -> %d", cur_state, next_state);

    }
}


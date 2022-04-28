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

#define INCLUDE_LOG_DEBUG 1
#include "log.h"

// status variables for the current state of the system
static heart_states_t cur_state;

// resets the data structures at initialization
void init_scheduler() {
    cur_state = STATE_WAITING;
    //LOG_INFO("Scheduler started");
}

// signals to bluetooth stack that external event occurred (5 second timer elapsed)
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

/*
 *
 * state machine for getting heart data
 *
 * evt = bluetooth event
 */
void heart_sensor_state_machine(sl_bt_msg_t* evt) {

    // initially assume system will remain in current state
    uint8_t next_state = cur_state;

    switch (cur_state) {

        case STATE_WAITING:
            //LOG_INFO("State Waiting for Readings");

            if (external_signal_event_match(evt, EVENT_CHECK_SENSOR)) {

                #ifdef LOW_POWER_MODE
                turn_on_heart_sensor();

                // needs to be on for 0.5 seconds before object detected in low power mode
                timer_wait_us_polled(500000);
                #endif

                read_heart_sensor();

                // acquire data if finger detected
                if ((get_heart_data_ptr()->finger_status == OBJECT_DETECTED) || get_heart_data_ptr()->finger_status == FINGER_DETECTED) {
                    LOG_INFO("State transition: waiting -> acquiring data");

                    displayPrintf(DISPLAY_ROW_ACTION, "Acquiring data...");
                    next_state = STATE_ACQUIRING_DATA;

                    timer_wait_us_IRQ(1000000);
                }
                // turn the sensor back off if nothing detected
                else {
                    #ifdef LOW_POWER_MODE
                    turn_off_heart_sensor();
                    #endif
                }

            }
            break;

        case STATE_ACQUIRING_DATA:
            //LOG_INFO("State Acquiring Readings");

            // 1 second timer finished
            if (external_signal_event_match(evt, EVENT_TIMER_EXPIRED)) {

                read_heart_sensor();

                // go back to waiting state if finger removed
                if ((get_heart_data_ptr()->finger_status == NOTHING_DETECTED)) {
                    LOG_INFO("State transition: acquiring data -> waiting");

                    displayPrintf(DISPLAY_ROW_ACTION, "Place Finger!");
                    next_state = STATE_WAITING;

                    #ifdef LOW_POWER_MODE
                    turn_off_heart_sensor();
                    #endif
                }
                // move to next state if confidence value high enough
                else if ((get_heart_data_ptr()->confidence > CONFIDENCE_THRESHOLD) && (get_heart_data_ptr()->finger_status == FINGER_DETECTED)) {
                    LOG_INFO("State transition: acquiring data -> returning data");

                    next_state = STATE_RETURNING_DATA;
                }

                timer_wait_us_IRQ(1000000);
            }

            break;

        case STATE_RETURNING_DATA:
            //LOG_INFO("State Returning Readings");

            // 1 second timer finished
            if (external_signal_event_match(evt, EVENT_TIMER_EXPIRED)) {

                read_heart_sensor();

                if ((get_heart_data_ptr()->finger_status == NOTHING_DETECTED)) {
                    displayPrintf(DISPLAY_ROW_ACTION, "Place Finger!");

                    #ifdef LOW_POWER_MODE
                    turn_off_heart_sensor();
                    #endif

                    next_state = STATE_WAITING;
                    LOG_INFO("State transition: returning data -> waiting");
                }
                // wait longer until returning data
                else if (get_heart_data_ptr()->confidence < CONFIDENCE_THRESHOLD) {
                    displayPrintf(DISPLAY_ROW_ACTION, "Acquiring data...");
                    next_state = STATE_ACQUIRING_DATA;
                    LOG_INFO("State transition: returning data -> acquiring data");
                }

                // save results, transmit data over BLE, and update the LCD display
                else {
                    get_ble_data_ptr()->heart_rate = get_heart_data_ptr()->heart_rate;
                    get_ble_data_ptr()->blood_oxygen = get_heart_data_ptr()->blood_oxygen;
                    get_ble_data_ptr()->confidence = get_heart_data_ptr()->confidence;

                    displayPrintf(DISPLAY_ROW_ACTION, "");
                    displayPrintf(DISPLAY_ROW_8, "Heart Rate: %d BPM", get_ble_data_ptr()->heart_rate);
                    displayPrintf(DISPLAY_ROW_9, "Blood Oxygen: %d%%", get_ble_data_ptr()->blood_oxygen);
                    displayPrintf(DISPLAY_ROW_10, "Confidence: %d%%", get_ble_data_ptr()->confidence);

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


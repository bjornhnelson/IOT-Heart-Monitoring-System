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

#include "em_letimer.h"

#define INCLUDE_LOG_DEBUG 1
#include "log.h"

// status variable for the current state of the system
static server_states_t cur_server_state;
static client_states_t cur_client_state;

// status variable for most recent event client needs to respond to
static client_events_t cur_client_event = EVENT_CLIENT_IDLE;


// 1809
static uuid_t htm_service = {
    .data = {0x09, 0x18},
    .len = 2
};

//
static uuid_t htm_characteristic = {
    .data = {0x1C, 0x2A},
    .len = 2
};

// resets the data structures at initialization
void init_scheduler() {
    cur_server_state = STATE_IDLE;
    cur_client_state = STATE_AWAITING_CONNECTION;
    LOG_INFO("Scheduler started");
}

// signals to bluetooth stack that external event occurred (3 second timer elapsed)
void scheduler_set_event_UF() {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    sl_bt_external_signal(EVENT_MEASURE_TEMP);
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

void scheduler_set_client_event(uint8_t event) {
    cur_client_event = event;
}

/*
 * check if bluetooth connection closed or indications not enabled
 * returns: boolean indicating whether to go to idle state
 */
uint8_t bluetooth_connection_errors() {
    return (!(get_ble_data_ptr()->connectionOpen) || !(get_ble_data_ptr()->tempIndicationsEnabled));
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

// client processing logic for handling states and events
void discovery_state_machine(sl_bt_msg_t* evt) {

    sl_status_t status;

    // Just a trick to hide a compiler warning about unused input parameter evt.
    (void) evt;

    // initially assume system will remain in current state
    uint8_t next_state = cur_client_state;

    switch (cur_client_state) {
        case STATE_AWAITING_CONNECTION:
            //LOG_INFO("** Awaiting Connection");
            // open event
            if (cur_client_event == EVENT_CONNECTION_OPENED) {
                LOG_INFO("STATE TRANSITION: Awaiting Connection -> Service Discovery");

                displayPrintf(DISPLAY_ROW_BTADDR2, "%x:%x:%x:%x:%x:%x",
                                  get_ble_data_ptr()->serverAddress.addr[0], get_ble_data_ptr()->serverAddress.addr[1], get_ble_data_ptr()->serverAddress.addr[2],
                                  get_ble_data_ptr()->serverAddress.addr[3], get_ble_data_ptr()->serverAddress.addr[4], get_ble_data_ptr()->serverAddress.addr[5]);

                LOG_INFO("** CALL sl_bt_gatt_discover_primary_services_by_uuid(%d, %d, %d %d", get_ble_data_ptr()->clientConnectionHandle, htm_service.len, htm_service.data[0], htm_service.data[1]);
                status = sl_bt_gatt_discover_primary_services_by_uuid(get_ble_data_ptr()->clientConnectionHandle, htm_service.len, htm_service.data);

                if (status != SL_STATUS_OK) {
                    LOG_ERROR("sl_bt_gatt_discover_primary_services_by_uuid: %d", status);
                }

                next_state = STATE_SERVICE_DISCOVERY;
            }
        break;

        case STATE_SERVICE_DISCOVERY:
            // service has been found
            //LOG_INFO("** Service Discovery");
            if (cur_client_event == EVENT_GATT_PROCEDURE_COMPLETED) {
                LOG_INFO("STATE TRANSITION: Service Discovery -> Characteristic Discovery");

                LOG_INFO("** CALL sl_bt_gatt_discover_characteristics_by_uuid(con handle %d, service handle %d, %d, %d %d", get_ble_data_ptr()->clientConnectionHandle, get_ble_data_ptr()->serviceHandle, htm_characteristic.len, htm_characteristic.data[0], htm_characteristic.data[1]);
                status = sl_bt_gatt_discover_characteristics_by_uuid(get_ble_data_ptr()->clientConnectionHandle, get_ble_data_ptr()->serviceHandle, htm_characteristic.len, htm_characteristic.data);

                if (status != SL_STATUS_OK) {
                    LOG_ERROR("sl_bt_gatt_discover_characteristics_by_uuid");
                }

                cur_client_event = EVENT_CLIENT_IDLE;
                next_state = STATE_CHARACTERISTIC_DISCOVERY;
            }

            if (cur_client_event == EVENT_CONNECTION_CLOSED) {
                displayPrintf(DISPLAY_ROW_BTADDR2, "");
                next_state = STATE_AWAITING_CONNECTION;
            }
        break;

        case STATE_CHARACTERISTIC_DISCOVERY:
            // characteristic has been found
            //LOG_INFO("** Characteristic Discovery");
            if (cur_client_event == EVENT_GATT_PROCEDURE_COMPLETED) {
                LOG_INFO("STATE TRANSITION: Characteristic Discovery -> Receiving Indications");

                LOG_INFO("*** CALL SET CHARACTERISTIC NOTIFICATION: conHandle = %d   charHandle = %d", get_ble_data_ptr()->clientConnectionHandle, get_ble_data_ptr()->characteristicHandle);

                status = sl_bt_gatt_set_characteristic_notification(get_ble_data_ptr()->clientConnectionHandle, get_ble_data_ptr()->characteristicHandle, sl_bt_gatt_indication);

                if (status != SL_STATUS_OK) {
                    LOG_ERROR("sl_bt_gatt_set_characteristic_notification");
                }

                displayPrintf(DISPLAY_ROW_CONNECTION, "Handling Indications");

                cur_client_event = EVENT_CLIENT_IDLE;
                next_state = STATE_RECEIVING_INDICATIONS;
            }

            if (cur_client_event == EVENT_CONNECTION_CLOSED) {
                displayPrintf(DISPLAY_ROW_BTADDR2, "");
                next_state = STATE_AWAITING_CONNECTION;
            }
        break;

        case STATE_RECEIVING_INDICATIONS:

            // call display temp here
            //LOG_INFO("** Receiving Indications");

            displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d", get_ble_data_ptr()->tempValue);

            // do nothing, keep receiving indications until receiving close event
            if (cur_client_event == EVENT_CONNECTION_CLOSED) {
                LOG_INFO("STATE TRANSITION: Receiving Indications -> Awaiting Connection");
                displayPrintf(DISPLAY_ROW_BTADDR2, "");
                next_state = STATE_AWAITING_CONNECTION;
            }
        break;

    }

    // do a state transition
    if (cur_client_state != next_state) {
        cur_client_state = next_state; // update global status variable
    }

}

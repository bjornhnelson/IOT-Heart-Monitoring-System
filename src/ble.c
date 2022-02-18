/*
 * ble.c
 *
 *  Created on: Feb 16, 2022
 *      Author: bjornnelson
 */

#include "ble.h"
#include "log.h"
#include "i2c.h"
#include "gatt_db.h"


// BLE private data
ble_data_struct_t ble_data;

// provides access to bluetooth data structure for other .c files
ble_data_struct_t* get_ble_data_ptr() {
    return (&ble_data);
}

sl_status_t status;


// called by state machine to send temperature to client
void ble_transmit_temp() {

    uint8_t temperature_buffer[5]; // why 5?
    uint8_t* ptr = temperature_buffer;


    uint8_t flags = 0x00;
    uint16_t temperature_in_c = get_temp();

    // convert into IEEE-11073 32-bit floating point value
    uint32_t temperature_flt = UINT32_TO_FLOAT(temperature_in_c*1000, -3);

    // put flag in the temperature buffer
    UINT8_TO_BITSTREAM(ptr, flags);

    // put temperature value in the temperature_buffer
    UINT32_TO_BITSTREAM(ptr, temperature_flt);

    // write value to GATT database
    status = sl_bt_gatt_server_write_attribute_value(
            gattdb_temperature_measurement, // characteristic from gatt_db.h
            0, // offset
            4, // length
            &temperature_in_c // pointer to value
            );

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_gatt_server_write_attribute_value");
    }

    // check if conditions are correct to send an indication
    if (ble_data.connectionOpen && ble_data.tempIndicationsEnabled && !(ble_data.indicationInFlight)) {

        status = sl_bt_gatt_server_send_indication(
                ble_data.connectionHandle,
                gattdb_temperature_measurement, // characteristic from gatt_db.h
                5, // value length
                &temperature_buffer[0] // value
                );

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_gatt_server_send_indication");
        }
        else {
            ble_data.indicationInFlight = true;
        }
    }
}

// This event indicates the device has started and the radio is ready
void ble_server_boot_event() {
    LOG_INFO("SYSTEM BOOT");

    uint8_t myAddressType;

    // Returns the unique BT device address
    status = sl_bt_system_get_identity_address(&(ble_data.myAddress), &myAddressType);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_system_get_identity_address");
    }

    // Creates an advertising set to use when the slave device wants to advertise its presence. The handle created by this call is only to be used for advertising API calls.
    status = sl_bt_advertiser_create_set(&(ble_data.advertisingSetHandle));

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_create_set");
    }

    uint32_t interval_min = 400; // Value = Time in ms / .625 ms = 250 / .625 = 400
    uint32_t interval_max = 400; // same math as min
    uint16_t duration = 0; // no duration limit, advertising continues until disabled
    uint8_t maxevents = 0; // no maximum number limit

    // Sets the timing to transmit advertising packets
    status = sl_bt_advertiser_set_timing(ble_data.advertisingSetHandle, interval_min, interval_max, duration, maxevents);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_set_timing");
    }

    // Tells the device to start sending advertising packets
    status = sl_bt_advertiser_start(ble_data.advertisingSetHandle, sl_bt_advertiser_general_discoverable, sl_bt_advertiser_connectable_scannable);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_start");
    }

}


// This event indicates that a new connection was opened
void ble_server_connection_opened_event(sl_bt_msg_t* evt) {
    LOG_INFO("CONNECTION OPENED");

    // update flags and save data
    ble_data.connectionOpen = true;
    ble_data.indicationInFlight = false;
    ble_data.connectionHandle = evt->data.evt_connection_opened.connection;

    // Stop advertising
    status = sl_bt_advertiser_stop(ble_data.advertisingSetHandle);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_stop");
    }

    uint16_t min_interval = 60; // Value = Time in ms / 1.25 ms = 75 / 1.25 = 60
    uint16_t max_interval = 60; // same math
    uint16_t latency = 3; // how many connection intervals the slave can skip - off air for up to 300 ms
    // value greater than (1 + slave latency) * (connection_interval * 2) = (1 + 3) * (75 * 2) = 4 * 150 = 600 ms
    // Value = Time / 10 ms = 600 / 10 = 60 -> use 80?
    uint16_t timeout = 80;
    uint16_t min_ce_length = 0; // default
    uint16_t max_ce_length = 0xffff; // no limitation

    // Send a request with a set of parameters to the master
    status = sl_bt_connection_set_parameters(ble_data.connectionHandle, min_interval, max_interval, latency, timeout, min_ce_length, max_ce_length);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_connection_set_parameters");
    }

}

// This event indicates that a connection was closed
void ble_server_connection_closed_event() {
    LOG_INFO("CONNECTION CLOSED");

    ble_data.connectionOpen = false;

    // Tells the device to start sending advertising packets
    status = sl_bt_advertiser_start(ble_data.advertisingSetHandle, sl_bt_advertiser_general_discoverable, sl_bt_advertiser_connectable_scannable);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_start");
    }

}

// Informational: triggered whenever the connection parameters are changed and at any time a connection is established
void ble_server_connection_parameters_event() {
    LOG_INFO("CONNECTION PARAMETERS CHANGED");

    // TODO: more logging

}

void ble_server_external_signal_event() {
    LOG_INFO("EXTERNAL SIGNAL EVENT");

    // handled in temperature_state_machine(), nothing to do here

}

/*
 * Handles 2 cases
 * 1. local CCCD value changed
 * 2. confirmation that indication was received
 */
void ble_server_characteristic_status_event(sl_bt_msg_t* evt) {
    LOG_INFO("CHARACTERISTIC STATUS EVENT");

    uint16_t characteristic = evt->data.evt_gatt_server_characteristic_status.characteristic;
    uint8_t status_flags = evt->data.evt_gatt_server_characteristic_status.status_flags;
    uint16_t client_config_flags = evt->data.evt_gatt_server_characteristic_status.client_config_flags;

    if (characteristic == gattdb_temperature_measurement) {
        if (status_flags == gatt_server_client_config) {
            if (client_config_flags == gatt_disable) {
                ble_data.tempIndicationsEnabled = false;
            }

            if (client_config_flags == gatt_indication) {
                ble_data.tempIndicationsEnabled = true;
            }
        }

        if (status_flags == gatt_server_confirmation) {
            ble_data.indicationInFlight = false;
        }
    }

}

void ble_server_indication_timeout_event() {
    LOG_INFO("INDICATION TIMEOUT OCCURRED");

    ble_data.indicationInFlight = false;
}

void handle_ble_event(sl_bt_msg_t* evt) {

    switch(SL_BT_MSG_ID(evt->header)) {

        // events common to servers and clients
        case sl_bt_evt_system_boot_id:
            ble_server_boot_event();
            break;

        case sl_bt_evt_connection_opened_id:
            ble_server_connection_opened_event(evt);
            break;

        case sl_bt_evt_connection_closed_id:
            ble_server_connection_closed_event();
            break;

        case sl_bt_evt_connection_parameters_id:
            ble_server_connection_parameters_event();
            break;

        // doesn't care about these events, state machine handles it
        case sl_bt_evt_system_external_signal_id:
            ble_server_external_signal_event();
            break;

        // events just for servers
        case sl_bt_evt_gatt_server_characteristic_status_id:
            ble_server_characteristic_status_event(evt);
            break;

        case sl_bt_evt_gatt_server_indication_timeout_id:
            ble_server_indication_timeout_event();
            break;

        // events just for clients
        // none right now

    }
}

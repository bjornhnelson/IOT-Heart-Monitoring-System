/*
 * ble.c
 *
 *  Created on: Feb 16, 2022
 *      Author: bjornnelson
 */

#include "ble.h"
#include "log.h"

// BLE private data
ble_data_struct_t ble_data;

// provides access to bluetooth data structure for other .c files
ble_data_struct_t* get_ble_data_ptr() {
    return (&ble_data);
}

sl_status_t status;


void ble_server_boot_event(sl_bt_msg_t* evt) {
    //LOG_INFO("SYSTEM BOOT");

    // Returns the unique BT device address
    uint8_t myAddressType;
    status = sl_bt_system_get_identity_address(&(ble_data.myAddress), &myAddressType);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_system_get_identity_address");
    }

    // Creates an advertising set to use when the slave device wants to advertise its presence. The handle created by this call is only to be used for advertising API calls.
    status = sl_bt_advertiser_create_set(&(ble_data.advertisingSetHandle));

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_create_set");
    }

    // Sets the timing to transmit advertising packets
    uint32_t interval_min = 400; // Value = Time in ms / .625 ms = 250 / .625 = 400
    uint32_t interval_max = 400; // same math as min
    uint16_t duration = 0; // no duration limit, advertising continues until disabled
    uint8_t maxevents = 0; // no maximum number limit
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

void ble_server_connection_opened_event(sl_bt_msg_t* evt) {
    //LOG_INFO("CONNECTION OPENED");

    ble_data.connectionOpen = true;
    ble_data.indicationInFlight = false;
    ble_data.connectionHandle = evt->data.evt_connection_opened.connection;

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
    status = sl_bt_connection_set_parameters(ble_data.connectionHandle, min_interval, max_interval, latency, timeout, min_ce_length, max_ce_length);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_connection_set_parameters");
    }

}

void ble_server_connection_closed_event(sl_bt_msg_t* evt) {
    //LOG_INFO("CONNECTION CLOSED");

    ble_data.connectionOpen = false;

    // Tells the device to start sending advertising packets
    status = sl_bt_advertiser_start(ble_data.advertisingSetHandle, sl_bt_advertiser_general_discoverable, sl_bt_advertiser_connectable_scannable);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_start");
    }

}

void ble_server_connection_parameters_event(sl_bt_msg_t* evt) {
    //LOG_INFO("sl_bt_evt_connection_parameters_id");

}

void ble_server_external_signal_event(sl_bt_msg_t* evt) {
    //LOG_INFO("sl_bt_evt_system_external_signal_id");

}

void ble_server_characteristic_status_event(sl_bt_msg_t* evt) {
    //LOG_INFO("sl_bt_evt_gatt_server_characteristic_status_id");

}

void ble_server_indication_timeout_event(sl_bt_msg_t* evt) {
    //LOG_INFO("sl_bt_evt_gatt_server_indication_timeout_id");

}

void handle_ble_event(sl_bt_msg_t* evt) {

    switch(SL_BT_MSG_ID(evt->header)) {

        // events common to servers and clients
        case sl_bt_evt_system_boot_id:
            ble_server_boot_event(evt);
            break;

        case sl_bt_evt_connection_opened_id:
            ble_server_connection_opened_event(evt);
            break;

        case sl_bt_evt_connection_closed_id:
            ble_server_connection_closed_event(evt);
            break;

        case sl_bt_evt_connection_parameters_id:
            ble_server_connection_parameters_event(evt);
            break;

        case sl_bt_evt_system_external_signal_id:
            ble_server_external_signal_event(evt);
            break;

        // events just for servers
        case sl_bt_evt_gatt_server_characteristic_status_id:
            ble_server_characteristic_status_event(evt);
            break;

        case sl_bt_evt_gatt_server_indication_timeout_id:
            ble_server_indication_timeout_event(evt);
            break;

        // events just for clients

    }
}

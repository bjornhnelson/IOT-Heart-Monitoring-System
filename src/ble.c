/*
 * ble.c
 *
 *  Created on: Feb 16, 2022
 *      Author: bjornnelson
 */

#include "ble.h"
#include "i2c.h"
#include "gatt_db.h"
#include "lcd.h"
#include "ble_device_type.h"
#include "scheduler.h"

// enable logging for errors
#define INCLUDE_LOG_DEBUG 1
#include "log.h"

sl_status_t status; // return variable for various api calls

ble_data_struct_t ble_data; // // BLE private data

// provides access to bluetooth data structure for other .c files
ble_data_struct_t* get_ble_data_ptr() {
    return (&ble_data);
}

// called by state machine to send temperature to client
void ble_transmit_temp() {

    uint8_t temperature_buffer[5]; // why 5?
    uint8_t* ptr = temperature_buffer;

    uint8_t flags = 0x00;
    uint8_t temperature_in_c = get_temp();

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
                ble_data.serverConnectionHandle,
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

        // show temperature on LCD
        displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d", temperature_in_c);
    }
}

// COMMON SERVER + CLIENT EVENTS BELOW

// This event indicates the device has started and the radio is ready
void ble_boot_event() {

#if DEVICE_IS_BLE_SERVER
    //LOG_INFO("SYSTEM BOOT");

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

    // enable the LCD
    displayInit();

    displayPrintf(DISPLAY_ROW_NAME, BLE_DEVICE_TYPE_STRING);
    displayPrintf(DISPLAY_ROW_ASSIGNMENT, "A6");

    displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");

    displayPrintf(DISPLAY_ROW_BTADDR, "%x:%x:%x:%x:%x:%x",
                  ble_data.myAddress.addr[0], ble_data.myAddress.addr[1], ble_data.myAddress.addr[2],
                  ble_data.myAddress.addr[3], ble_data.myAddress.addr[4], ble_data.myAddress.addr[5]);

#else

    // save server address
    bd_addr server_addr  = SERVER_BT_ADDRESS;

    ble_data.serverAddress = server_addr;

    uint8_t myAddressType;
    status = sl_bt_system_get_identity_address(&(ble_data.clientAddress), &myAddressType);

    uint8_t phys = 1;
    status = sl_bt_scanner_set_mode(phys, 0); // 1M PHY, passive scanning

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_scanner_set_mode");
    }

    // Time = Value * .625 ms
    uint16_t scan_interval = 80; // 50 ms
    uint16_t scan_window = 40; // 25 ms
    status = sl_bt_scanner_set_timing(phys, scan_interval, scan_window);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_scanner_set_timing");
    }

    // Value = Time / 1.25
    uint16_t min_interval = 60; // 75 ms
    uint16_t max_interval = 60; // 75 ms

    uint16_t latency = 4;

    uint16_t timeout = (1 + latency) * (max_interval * 2) + max_interval; // 660
    uint16_t min_ce_length = 0;
    uint16_t max_ce_length = 4;
    status = sl_bt_connection_set_default_parameters(min_interval, max_interval, latency, timeout, min_ce_length, max_ce_length);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_connection_set_default_parameters");
    }

    // check 2nd parameter, not sure what is best
    status = sl_bt_scanner_start(phys, sl_bt_scanner_discover_generic);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_scanner_start");
    }

    // enable the LCD
    displayInit();

    displayPrintf(DISPLAY_ROW_NAME, BLE_DEVICE_TYPE_STRING);
    displayPrintf(DISPLAY_ROW_ASSIGNMENT, "A7");

    displayPrintf(DISPLAY_ROW_CONNECTION, "Discovering");

    displayPrintf(DISPLAY_ROW_BTADDR, "%x:%x:%x:%x:%x:%x",
                  ble_data.clientAddress.addr[0], ble_data.clientAddress.addr[1], ble_data.clientAddress.addr[2],
                  ble_data.clientAddress.addr[3], ble_data.clientAddress.addr[4], ble_data.clientAddress.addr[5]);


#endif

}

/*
 * This event indicates that a new connection was opened
 *
 * evt = event that occurred
 */
void ble_connection_opened_event(sl_bt_msg_t* evt) {

#if DEVICE_IS_BLE_SERVER
    //LOG_INFO("CONNECTION OPENED");

    // update flags and save data
    ble_data.connectionOpen = true;
    ble_data.indicationInFlight = false;
    ble_data.serverConnectionHandle = evt->data.evt_connection_opened.connection;

    // Stop advertising
    status = sl_bt_advertiser_stop(ble_data.advertisingSetHandle);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_stop");
    }

    ble_data.min_interval = 60; // Value = Time in ms / 1.25 ms = 75 / 1.25 = 60
    ble_data.max_interval = 60; // same math
    ble_data.latency = 3; // how many connection intervals the slave can skip - off air for up to 300 ms

    // value greater than (1 + slave latency) * (connection_interval * 2) = (1 + 3) * (75 * 2) = 4 * 150 = 600 ms
    // Value = Time / 10 ms = 600 / 10 = 60 -> use 80
    ble_data.timeout = 80;

    uint16_t min_ce_length = 0; // default
    uint16_t max_ce_length = 0xffff; // no limitation

    // Send a request with a set of parameters to the master
    status = sl_bt_connection_set_parameters(ble_data.serverConnectionHandle,
                                             ble_data.min_interval,
                                             ble_data.max_interval,
                                             ble_data.latency,
                                             ble_data.timeout,
                                             min_ce_length,
                                             max_ce_length);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_connection_set_parameters");
    }

    displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");

#else
    scheduler_set_client_event(EVENT_CONNECTION_OPENED);

    ble_data.clientConnectionHandle = evt->data.evt_connection_opened.connection;

    LOG_INFO("CLIENT CONNECTION HANDLE: %d", ble_data.clientConnectionHandle);

    displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");

    // already did default params
    //sl_bt_connection_set_parameters();


#endif

}

// This event indicates that a connection was closed
void ble_connection_closed_event() {

#if DEVICE_IS_BLE_SERVER
    //LOG_INFO("CONNECTION CLOSED");

    ble_data.connectionOpen = false;

    // Tells the device to start sending advertising packets
    status = sl_bt_advertiser_start(ble_data.advertisingSetHandle, sl_bt_advertiser_general_discoverable, sl_bt_advertiser_connectable_scannable);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_start");
    }

    displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");
    displayPrintf(DISPLAY_ROW_TEMPVALUE, "");

#else
    scheduler_set_client_event(EVENT_CONNECTION_CLOSED);

    uint8_t phys = 1;
    // check 2nd parameter, not sure what is best
    status = sl_bt_scanner_start(phys, sl_bt_scanner_discover_generic);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_scanner_start");
    }

    displayPrintf(DISPLAY_ROW_CONNECTION, "Discovering");
    displayPrintf(DISPLAY_ROW_TEMPVALUE, "");

#endif

}

// Informational: triggered whenever the connection parameters are changed and at any time a connection is established
void ble_connection_parameters_event(sl_bt_msg_t* evt) {

    // Just a trick to hide a compiler warning about unused input parameter evt.
    (void) evt;

#if DEVICE_IS_BLE_SERVER
    //LOG_INFO("CONNECTION PARAMETERS CHANGED");

    // Just a trick to hide a compiler warning about unused input parameter evt.
    (void) evt;

    // log interval, latency, and timeout values from **client**
    /*LOG_INFO("interval = %d, latency = %d, timeout = %d",
             evt->data.evt_connection_parameters.interval,
             evt->data.evt_connection_parameters.latency,
             evt->data.evt_connection_parameters.timeout);*/

#else
    // hello
#endif

}

// handles external events
void ble_external_signal_event() {

#if DEVICE_IS_BLE_SERVER
    //LOG_INFO("EXTERNAL SIGNAL EVENT");

    // nothing to do here, external events handled in temperature_state_machine()

#else
    // hello
#endif

}

void ble_system_soft_timer_event() {
    // do nothing
}

// SERVER EVENTS BELOW

/*
 * Handles 2 cases
 * 1. local CCCD value changed
 * 2. confirmation that indication was received
 *
 * evt = event that occurred
 */
void ble_server_characteristic_status_event(sl_bt_msg_t* evt) {
    //LOG_INFO("CHARACTERISTIC STATUS EVENT");

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

// Possible event from never receiving confirmation for previously transmitted indication
void ble_server_indication_timeout_event() {
    //LOG_INFO("INDICATION TIMEOUT OCCURRED");

    ble_data.indicationInFlight = false;
}

// CLIENT EVENTS BELOW

bool addressesMatch(bd_addr a1, bd_addr a2) {

    //LOG_INFO("Addr 1: %x:%x:%x:%x:%x:%x", a1.addr[0], a1.addr[1], a1.addr[2], a1.addr[3], a1.addr[4], a1.addr[5]);
    //LOG_INFO("Addr 2: %x:%x:%x:%x:%x:%x", a2.addr[0], a2.addr[1], a2.addr[2], a2.addr[3], a2.addr[4], a2.addr[5]);

    return ((a1.addr[0] == a2.addr[0]) &&
            (a1.addr[1] == a2.addr[1]) &&
            (a1.addr[2] == a2.addr[2]) &&
            (a1.addr[3] == a2.addr[3]) &&
            (a1.addr[4] == a2.addr[4]) &&
            (a1.addr[5] == a2.addr[5]));
}

void ble_client_scanner_scan_report_event(sl_bt_msg_t* evt) {

    //LOG_INFO("SCAN REPORT: %d", evt->data.evt_scanner_scan_report.address_type);

    uint8_t connection;

    // check conditions for opening connection - bd_addr, packet_type and address_type
    if (addressesMatch(evt->data.evt_scanner_scan_report.address, ble_data.serverAddress))
    /*if (addressesMatch(evt->data.evt_scanner_scan_report.address, ble_data.myAddress) &&
            //(evt->data.evt_scanner_scan_report.packet_type == 1) &&
            (evt->data.evt_scanner_scan_report.address_type == 0)) // public address*/
    {
        LOG_INFO("ADDRESSES MATCH!");

        status = sl_bt_scanner_stop();

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_scanner_stop");
        }


        // TODO: check these params
        LOG_INFO("Called connection open");
        status = sl_bt_connection_open(ble_data.serverAddress, evt->data.evt_scanner_scan_report.address_type, 0x01, &connection);

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_connection_open");
        }

    }

}

void ble_client_gatt_procedure_completed_event() {
    scheduler_set_client_event(EVENT_GATT_PROCEDURE_COMPLETED);
}

void ble_client_gatt_service_event(sl_bt_msg_t* evt) {

    ble_data.serviceHandle = evt->data.evt_gatt_service.service;
}

void ble_client_gatt_characteristic_event(sl_bt_msg_t* evt) {

    ble_data.characteristicHandle = evt->data.evt_gatt_characteristic.characteristic;
}

void ble_client_gatt_characteristic_value_event(sl_bt_msg_t* evt) {

    // if char handle and opcode is expected, save value and send confirmation

    if ((evt->data.evt_gatt_characteristic_value.att_opcode == sl_bt_gatt_handle_value_indication) &&
            (evt->data.evt_gatt_characteristic_value.characteristic == ble_data.characteristicHandle)) {

        ble_data.characteristicValue.len = evt->data.evt_gatt_characteristic_value.value.len;

        for (int i=0; i<ble_data.characteristicValue.len; i++) {
            ble_data.characteristicValue.data[i] = evt->data.evt_gatt_characteristic_value.value.data[i];
        }

        sl_bt_gatt_send_characteristic_confirmation(ble_data.clientConnectionHandle);
    }


}

/*
 * event handler for various bluetooth events
 *
 * evt = event that occurred
 */
void handle_ble_event(sl_bt_msg_t* evt) {

    switch(SL_BT_MSG_ID(evt->header)) {

        // events common to servers and clients
        case sl_bt_evt_system_boot_id:
            ble_boot_event();
            break;

        case sl_bt_evt_connection_opened_id:
            ble_connection_opened_event(evt);
            break;

        case sl_bt_evt_connection_closed_id:
            ble_connection_closed_event();
            break;

        case sl_bt_evt_connection_parameters_id:
            ble_connection_parameters_event(evt);
            break;

        case sl_bt_evt_system_external_signal_id:
            ble_external_signal_event();
            break;

        case sl_bt_evt_system_soft_timer_id:
            ble_system_soft_timer_event();
            break;

        // events just for servers
        case sl_bt_evt_gatt_server_characteristic_status_id:
            ble_server_characteristic_status_event(evt);
            break;

        case sl_bt_evt_gatt_server_indication_timeout_id:
            ble_server_indication_timeout_event();
            break;


        // events just for clients
        case sl_bt_evt_scanner_scan_report_id:
            ble_client_scanner_scan_report_event(evt);
        break;

        case sl_bt_evt_gatt_procedure_completed_id:
            ble_client_gatt_procedure_completed_event();
        break;

        case sl_bt_evt_gatt_service_id:
            ble_client_gatt_service_event(evt);
        break;

        case sl_bt_evt_gatt_characteristic_id:
            ble_client_gatt_characteristic_event(evt);
        break;

        case sl_bt_evt_gatt_characteristic_value_id:
            ble_client_gatt_characteristic_value_event(evt);
        break;


    }
}

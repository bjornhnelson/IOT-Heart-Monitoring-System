/*
 * ble.c
 *
 *  Created on: Feb 16, 2022
 *      Author: bjornnelson
 */

#include "ble.h"

// BLE private data
ble_data_struct_t ble_data;

// provides access to bluetooth data structure for other .c files
ble_data_struct_t* get_ble_data() {
    return &ble_data;
}

// events to handle
/*
 * sl_bt_evt_system_boot_id
 * sl_bt_evt_connection_opened_id
 * sl_bt_evt_connection_closed_id
 * sl_bt_evt_connection_parameters_id
 * sl_bt_evt_system_external_signal_id
 * sl_bt_evt_gatt_server_characteristic_status_id
 * sl_bt_evt_gatt_server_indication_timeout_id
 */

void ble_server_boot_event() {
    //LOG_INFO("sl_bt_evt_system_boot_id event");

}

void ble_server_connection_opened_event() {
    //LOG_INFO("sl_bt_evt_connection_opened_id");

}

void ble_server_connection_closed_event() {
    //LOG_INFO("sl_bt_evt_connection_closed_id");

}

void ble_server_connection_parameters_event() {
    //LOG_INFO("sl_bt_evt_connection_parameters_id");

}

void ble_server_external_signal_event() {
    //LOG_INFO("sl_bt_evt_system_external_signal_id");

}

void ble_server_characteristic_status_event() {
    //LOG_INFO("sl_bt_evt_gatt_server_characteristic_status_id");

}

void ble_server_indication_timeout_event() {
    //LOG_INFO("sl_bt_evt_gatt_server_indication_timeout_id");

}

void handle_ble_event(sl_bt_msg_t* evt) {

    switch(SL_BT_MSG_ID(evt->header)) {

        case sl_bt_evt_system_boot_id:
            ble_server_boot_event();
            break;

        case sl_bt_evt_connection_opened_id:
            ble_server_connection_opened_event();
            break;

        case sl_bt_evt_connection_closed_id:
            ble_server_connection_closed_event();
            break;

        case sl_bt_evt_connection_parameters_id:
            ble_server_connection_parameters_event();
            break;

        case sl_bt_evt_system_external_signal_id:
            ble_server_external_signal_event();
            break;

        case sl_bt_evt_gatt_server_characteristic_status_id:
            ble_server_characteristic_status_event();
            break;

        case sl_bt_evt_gatt_server_indication_timeout_id:
            ble_server_indication_timeout_event();
            break;

    }
}

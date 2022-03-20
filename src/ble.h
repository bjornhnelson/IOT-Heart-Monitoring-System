/*
 * ble.h
 *
 *  Created on: Feb 16, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_BLE_H_
#define SRC_BLE_H_

#include "stdint.h"
#include "stdbool.h"
#include "sl_bgapi.h"
#include "sl_bt_api.h"

#define UINT8_TO_BITSTREAM(p, n) { *(p)++ = (uint8_t)(n); }

#define UINT32_TO_BITSTREAM(p, n) { *(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                    *(p)++ = (uint8_t)((n) >> 16); *(p)++ = (uint8_t)((n) >> 24); }

#define UINT32_TO_FLOAT(m, e) (((uint32_t)(m) & 0x00FFFFFFU) | (uint32_t)((int32_t)(e) << 24))

// BLE Data Structure, save all of our private BT data in here.
// Modern C (circa 2021 does it this way)
// typedef ble_data_struct_t is referred to as an anonymous struct definition
typedef struct {

    // values that are common to servers and clients
    bd_addr myAddress;

    // values unique for server
    uint8_t advertisingSetHandle; // The advertising set handle allocated from Bluetooth stack.
    bool connectionOpen;
    bool indicationInFlight;
    uint8_t serverConnectionHandle;
    bool tempIndicationsEnabled;
    uint16_t min_interval;
    uint16_t max_interval;
    uint16_t latency;
    uint16_t timeout;

    bool bonded;
    bool pbIndicationsEnabled;
    bool passkeyConfirm;
    bool pb0Pressed;
    bool pb1Pressed;

    // values unique for client
    uint8_t clientConnectionHandle;
    uint32_t htmServiceHandle;
    uint16_t htmCharacteristicHandle;
    uint32_t pbServiceHandle;
    uint16_t pbCharacteristicHandle;
    uint8array htmCharacteristicValue;
    bd_addr clientAddress;
    bd_addr serverAddress;
    int32_t tempValue;

} ble_data_struct_t;

// data structure for storing UUIDs
typedef struct {
    uint8_t data[16];
    uint8_t len;
} uuid_t;

extern uuid_t htm_service;
extern uuid_t htm_characteristic;
extern uuid_t pb_service;
extern uuid_t pb_characteristic;

ble_data_struct_t* get_ble_data_ptr();

void ble_transmit_button_state();
void ble_transmit_temp();

// common server + client events
void ble_boot_event();
void ble_connection_opened_event(sl_bt_msg_t* evt);
void ble_connection_closed_event();
void ble_connection_parameters_event(sl_bt_msg_t* evt);
void ble_external_signal_event(sl_bt_msg_t* evt);
void ble_system_soft_timer_event();

// server events
void ble_server_characteristic_status_event(sl_bt_msg_t* evt);
void ble_server_indication_timeout_event();
void ble_server_sm_confirm_bonding_event();
void ble_sm_confirm_passkey_id(sl_bt_msg_t* evt);
void ble_sm_bonded_id();
void ble_sm_bonding_failed_id();

// client events
void ble_client_scanner_scan_report_event(sl_bt_msg_t* evt);
void ble_client_gatt_procedure_completed_event();
void ble_client_gatt_service_event(sl_bt_msg_t* evt);
void ble_client_gatt_characteristic_event(sl_bt_msg_t* evt);
void ble_client_gatt_characteristic_value_event(sl_bt_msg_t* evt);

// event responder
void handle_ble_event(sl_bt_msg_t* event);

#endif /* SRC_BLE_H_ */

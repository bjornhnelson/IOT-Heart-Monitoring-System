/*
 * ble.h
 *
 *  Created on: Feb 16, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_BLE_H_
#define SRC_BLE_H_

#include "stdint.h"
#include "sl_bgapi.h"

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
    // The advertising set handle allocated from Bluetooth stack.
    uint8_t advertisingSetHandle;

    // values unique for client


} ble_data_struct_t;

#endif /* SRC_BLE_H_ */

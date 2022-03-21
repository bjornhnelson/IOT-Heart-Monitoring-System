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
#include "math.h"
#include "gpio.h"

// enable logging for errors
#define INCLUDE_LOG_DEBUG 1
#include "log.h"


// CIRCULAR BUFFER CODE

#define QUEUE_DEPTH      (16)

typedef struct {
    uint16_t charHandle; // Char handle from gatt_db.h
    size_t bufferLength; // Length of buffer in bytes to send

    // Need space for HTM (5 bytes) and button_state (2 bytes) indications, buffer[0] holds the flag byte
    uint8_t buffer[5]; // The actual data buffer for the indication.

} queue_struct_t;

// Declare memory for the queue/buffer/fifo, and the write and read pointers
queue_struct_t   my_queue[QUEUE_DEPTH]; // the queue
uint32_t         wptr = 0;              // write pointer
uint32_t         rptr = 0;              // read pointer
int num_queue_entries = 0; // how many elements are in the buffer


// ---------------------------------------------------------------------
// Private function used only by this .c file.
// compute next ptr value
// Isolation of functionality: This defines "how" a pointer advances.
// ---------------------------------------------------------------------
static uint32_t nextPtr(uint32_t ptr) {

  if (ptr + 1 == QUEUE_DEPTH) {
      return 0; // wrap back to beginning of queue
  }
  else {
      return ptr + 1; // advance by 1
  }

}


// status check for enqueuing
bool queue_full() {
    if (num_queue_entries == QUEUE_DEPTH)
        return true;
    else
        return false;
}


// status check for dequeuing
bool queue_empty() {
    if (num_queue_entries == 0)
        return true;
    else
        return false;
}


// ---------------------------------------------------------------------
// Public function
// This function writes an entry to the queue.
// Returns false if successful or true if writing to a full fifo.
// ---------------------------------------------------------------------
bool write_queue(uint16_t charHandle, size_t bufferLength, uint8_t* buffer) {

  // nothing enqueued, fifo full
  if (queue_full()) {
      return true;
  }

  // element enqueued
  my_queue[wptr].charHandle = charHandle;
  my_queue[wptr].bufferLength = bufferLength;

  for (int i=0; i<5; i++) {
      my_queue[wptr].buffer[i] = buffer[i];
  }

  wptr = nextPtr(wptr);
  num_queue_entries++;

  return false;

}


// ---------------------------------------------------------------------
// Public function
// This function reads an entry from the queue.
// Returns false if successful or true if reading from an empty fifo.
// ---------------------------------------------------------------------
bool read_queue(uint16_t* charHandle, size_t* bufferLength, uint8_t* buffer) {

  // nothing dequeued, fifo empty
  if (queue_empty()) {
      return true;
  }

  // element dequeued
  *charHandle = my_queue[rptr].charHandle;
  *bufferLength = my_queue[rptr].bufferLength;

  for (int i=0; i<5; i++) {
      buffer[i] = my_queue[rptr].buffer[i];
  }

  rptr = nextPtr(rptr);
  num_queue_entries--;

  return false;

}


// END OF CIRCULAR BUFFER CODE


// 2nd soft timer setup
#define QUEUE_HANDLE 3

// 3268 = 0.1 sec = 100 ms
// 1635 = 50 ms
// 327 = 0.01 sec = 10 ms
#define QUEUE_TIMER_INTERVAL 1635

sl_status_t status; // return variable for various api calls

ble_data_struct_t ble_data; // // BLE private data

// UUID = 1809
uuid_t htm_service = {
    .data = {0x09, 0x18}, // little endian
    .len = 2
};

// UUID = 2A1C
uuid_t htm_characteristic = {
    .data = {0x1C, 0x2A}, // little endian
    .len = 2
};

// UUID = 00000001-38c8-433e-87ec-652a2d136289
uuid_t pb_service = {
    .data = {0x89, 0x62, 0x13, 0x2d, 0x2a, 0x65, 0xec, 0x87, 0x3e, 0x43, 0xc8, 0x38, 0x01, 0x00, 0x00, 0x00}, // little endian
    .len = 16
};

// UUID = 00000002-38c8-433e-87ec-652a2d136289
uuid_t pb_characteristic = {
    .data = {0x89, 0x62, 0x13, 0x2d, 0x2a, 0x65, 0xec, 0x87, 0x3e, 0x43, 0xc8, 0x38, 0x02, 0x00, 0x00, 0x00}, // little endian
    .len = 16
};


// provides access to bluetooth data structure for other .c files
ble_data_struct_t* get_ble_data_ptr() {
    return (&ble_data);
}

// Private function, original from Dan Walkes. I fixed a sign extension bug.
// We'll need this for Client A7 assignment to convert health thermometer
// indications back to an integer. Convert IEEE-11073 32-bit float to signed integer.
static int32_t FLOAT_TO_INT32(const uint8_t *value_start_little_endian) {
    uint8_t signByte = 0;
    int32_t mantissa;

    // input data format is:
    // [0] = flags byte
    // [3][2][1] = mantissa (2's complement)
    // [4] = exponent (2's complement)

    // BT value_start_little_endian[0] has the flags byte
    int8_t exponent = (int8_t)value_start_little_endian[4];

    // sign extend the mantissa value if the mantissa is negative
    if (value_start_little_endian[3] & 0x80) { // msb of [3] is the sign of the mantissa
        signByte = 0xFF;
    }

    mantissa = (int32_t) (value_start_little_endian[1] << 0) |
            (value_start_little_endian[2] << 8) |
            (value_start_little_endian[3] << 16) |
            (signByte << 24);

    // value = 10^exponent * mantissa, pow() returns a double type
    return (int32_t) (pow(10, exponent) * mantissa);
}

// OTHER FUNCTIONS

/*
 * check if 2 bluetooth addresses match
 *
 * a1 = address 1
 * a2 = address 2
 *
 * returns: true or false
 */
static bool addressesMatch(bd_addr a1, bd_addr a2) {

    return ((a1.addr[0] == a2.addr[0]) &&
            (a1.addr[1] == a2.addr[1]) &&
            (a1.addr[2] == a2.addr[2]) &&
            (a1.addr[3] == a2.addr[3]) &&
            (a1.addr[4] == a2.addr[4]) &&
            (a1.addr[5] == a2.addr[5]));
}

// called by external signal to send push button indications to client
void ble_transmit_button_state() {

    uint8_t pb_buffer[2];
    pb_buffer[0] = 0; // flags byte
    pb_buffer[1] = ble_data.pb0Pressed;

    // check if conditions are correct to send an indication
    // connection open AND button_state indications are enabled AND bonded
    if (ble_data.connectionOpen && ble_data.pbIndicationsEnabled && ble_data.bonded) {

        // no indication in flight, send right away
        if (!(ble_data.indicationInFlight)) {
            status = sl_bt_gatt_server_send_indication(
                    ble_data.serverConnectionHandle,
                    gattdb_button_state, // characteristic from gatt_db.h
                    2, // value length
                    pb_buffer // value
                    );


            if (status != SL_STATUS_OK) {
                LOG_ERROR("PB sl_bt_gatt_server_send_indication");
            }
            else {
                ble_data.indicationInFlight = true;
            }
        }
        else { // put into circular buffer, send later
            CORE_DECLARE_IRQ_STATE;
            CORE_ENTER_CRITICAL();
            write_queue(gattdb_button_state, 2, pb_buffer);
            CORE_EXIT_CRITICAL();
        }
    }
}

// called by state machine to send temperature indications to client
void ble_transmit_temp() {

    uint8_t temperature_buffer[5]; // why 5?
    uint8_t* ptr = temperature_buffer;

    uint8_t flags = 0x00;
    uint8_t temperature_in_c = get_temp();
    displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d", temperature_in_c);

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

    temperature_in_c += 5;

    if (status != SL_STATUS_OK) {
        LOG_ERROR("TEMP sl_bt_gatt_server_write_attribute_value");
    }

    // check if conditions are correct to send an indication
    if (ble_data.connectionOpen && ble_data.tempIndicationsEnabled) {

        // no indication in flight, send right away
        if (!(ble_data.indicationInFlight)) {
            status = sl_bt_gatt_server_send_indication(
                    ble_data.serverConnectionHandle,
                    gattdb_temperature_measurement, // characteristic from gatt_db.h
                    5, // value length
                    temperature_buffer // value
                    );

            if (status != SL_STATUS_OK) {
                LOG_ERROR("TEMP sl_bt_gatt_server_send_indication");
            }
            else {
                ble_data.indicationInFlight = true;
            }
        }
        else { // put into circular buffer, send later
            CORE_DECLARE_IRQ_STATE;
            CORE_ENTER_CRITICAL();
            write_queue(gattdb_temperature_measurement, 5, temperature_buffer);
            CORE_EXIT_CRITICAL();
        }
    }
}

// COMMON SERVER + CLIENT EVENTS BELOW

// This event indicates the device has started and the radio is ready
void ble_boot_event() {

#if DEVICE_IS_BLE_SERVER
    //LOG_INFO("SYSTEM BOOT");

    status = sl_bt_sm_delete_bondings();

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_sm_delete_bondings");
    }

    ble_data.bonded = false;
    ble_data.pb0Pressed = false;
    ble_data.passkeyConfirm = false;

    // initialize the security manager
    uint8_t flags = 0x0F; // Bonding requires MITM protection, Encryption requires bonding, Secure connections only, Bonding requests need to be confirmed
    status = sl_bt_sm_configure(flags, sm_io_capability_displayyesno);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_sm_configure");
    }

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
    displayInit(); // starts a 1 second soft timer

    displayPrintf(DISPLAY_ROW_NAME, BLE_DEVICE_TYPE_STRING);
    displayPrintf(DISPLAY_ROW_ASSIGNMENT, "A9");

    displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");

    displayPrintf(DISPLAY_ROW_BTADDR, "%x:%x:%x:%x:%x:%x",
                  ble_data.myAddress.addr[0], ble_data.myAddress.addr[1], ble_data.myAddress.addr[2],
                  ble_data.myAddress.addr[3], ble_data.myAddress.addr[4], ble_data.myAddress.addr[5]);

#else

    status = sl_bt_sm_delete_bondings();

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_sm_delete_bondings");
    }

    ble_data.bonded = false;
    ble_data.pb0Pressed = false;
    ble_data.pb1Pressed = false;
    ble_data.passkeyConfirm = false;
    ble_data.readInFlight = false;

    // initialize the security manager
    uint8_t flags = 0x0F; // Bonding requires MITM protection, Encryption requires bonding, Secure connections only, Bonding requests need to be confirmed
    status = sl_bt_sm_configure(flags, sm_io_capability_displayyesno);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_sm_configure");
    }

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

    status = sl_bt_scanner_start(phys, sl_bt_scanner_discover_generic);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_scanner_start");
    }

    // enable the LCD
    displayInit();

    displayPrintf(DISPLAY_ROW_NAME, BLE_DEVICE_TYPE_STRING);
    displayPrintf(DISPLAY_ROW_ASSIGNMENT, "A9");

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

    // start a 2nd soft timer for circular queue checks
    status = sl_bt_system_set_soft_timer(QUEUE_TIMER_INTERVAL, QUEUE_HANDLE, false);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_system_set_soft_timer 2");
    }

#else
    //scheduler_set_client_event(EVENT_CONNECTION_OPENED);

    ble_data.clientConnectionHandle = evt->data.evt_connection_opened.connection;

    displayPrintf(DISPLAY_ROW_CONNECTION, "Connected");
    displayPrintf(DISPLAY_ROW_BTADDR2, "%x:%x:%x:%x:%x:%x",
                  get_ble_data_ptr()->serverAddress.addr[0], get_ble_data_ptr()->serverAddress.addr[1], get_ble_data_ptr()->serverAddress.addr[2],
                  get_ble_data_ptr()->serverAddress.addr[3], get_ble_data_ptr()->serverAddress.addr[4], get_ble_data_ptr()->serverAddress.addr[5]);

#endif

}

// This event indicates that a connection was closed
void ble_connection_closed_event() {

#if DEVICE_IS_BLE_SERVER
    //LOG_INFO("CONNECTION CLOSED");

    status = sl_bt_sm_delete_bondings();

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_sm_delete_bondings");
    }

    ble_data.connectionOpen = false;
    ble_data.bonded = false;

    // Tells the device to start sending advertising packets
    status = sl_bt_advertiser_start(ble_data.advertisingSetHandle, sl_bt_advertiser_general_discoverable, sl_bt_advertiser_connectable_scannable);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_advertiser_start");
    }

    gpioLed0SetOff();
    gpioLed1SetOff();

    displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");
    displayPrintf(DISPLAY_ROW_TEMPVALUE, "");

#else
    //scheduler_set_client_event(EVENT_CONNECTION_CLOSED);

    status = sl_bt_sm_delete_bondings();

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_sm_delete_bondings");
    }

    uint8_t phys = 1;
    status = sl_bt_scanner_start(phys, sl_bt_scanner_discover_generic);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_scanner_start");
    }

    gpioLed0SetOff();
    gpioLed1SetOff();

    displayPrintf(DISPLAY_ROW_CONNECTION, "Discovering");
    displayPrintf(DISPLAY_ROW_TEMPVALUE, "");
    displayPrintf(DISPLAY_ROW_BTADDR2, "");

#endif

}

// Informational: triggered whenever the connection parameters are changed and at any time a connection is established
void ble_connection_parameters_event(sl_bt_msg_t* evt) {

    // Just a trick to hide a compiler warning about unused input parameter evt.
    (void) evt;

    //LOG_INFO("CONNECTION PARAMETERS CHANGED");

    // log interval, latency, and timeout values from **client**
    /*LOG_INFO("interval = %d, latency = %d, timeout = %d",
             evt->data.evt_connection_parameters.interval,
             evt->data.evt_connection_parameters.latency,
             evt->data.evt_connection_parameters.timeout);*/

}

// handles external events
void ble_external_signal_event(sl_bt_msg_t* evt) {

#if DEVICE_IS_BLE_SERVER
    //LOG_INFO("EXTERNAL SIGNAL EVENT");

    // handle pairing process
    if ((evt->data.evt_system_external_signal.extsignals == EVENT_PB0) && (ble_data.passkeyConfirm == true) && ble_data.pb0Pressed) {

        status = sl_bt_sm_passkey_confirm(ble_data.serverConnectionHandle, 1);

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_sm_passkey_confirm");
        }
        ble_data.passkeyConfirm = false;
    }

    // convert bool to int, update LCD
    uint8_t button_state;
    if (ble_data.pb0Pressed) {
        button_state = 1;
        displayPrintf(DISPLAY_ROW_9, "Button Pressed");
    }
    else {
        button_state = 0;
        displayPrintf(DISPLAY_ROW_9, "Button Released");
    }

    // whenever a change in button state occurs (pressed or released), update the GATT database
    if ((evt->data.evt_system_external_signal.extsignals == EVENT_PB0)) {

        // parameters: attribute from gatt_db.h, value offset, array length, value
        status = sl_bt_gatt_server_write_attribute_value(gattdb_button_state, 0, 1, &button_state);

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_gatt_server_write_attribute_value");
        }

        // send an indication
        ble_transmit_button_state();
    }

#else

    if (evt->data.evt_system_external_signal.extsignals == EVENT_PB1) { // when button 1 is pressed (rising edge only)

        // read from gatt database
        if (!ble_data.pb0Pressed) {
            // TODO: add check for if read already in flight, ignore 2nd press
            LOG_INFO("Issuing read");

            if (!ble_data.readInFlight) {
                status = sl_bt_gatt_read_characteristic_value(ble_data.clientConnectionHandle, ble_data.pbCharacteristicHandle);

                if (status != SL_STATUS_OK) {
                    LOG_ERROR("sl_bt_gatt_read_characteristic_value: %d", status);
                }

                ble_data.readInFlight = true; // ignore subsequent button presses while a read is still in flight
            }


            // display updated with sl_bt_evt_gatt_characteristic_value event after read
        }

        // toggle indications on/off
        else {

            // tell server to disable indications
            if (ble_data.pbIndicationsEnabled) {
                //LOG_INFO("Indications toggled OFF");
                ble_data.pbIndicationsEnabled = false;
                status = sl_bt_gatt_set_characteristic_notification(ble_data.clientConnectionHandle, ble_data.pbCharacteristicHandle, gatt_disable);

                if (status != SL_STATUS_OK) {
                    LOG_ERROR("sl_bt_gatt_set_characteristic_notification 1");
                }
            }

            // tell server to enable indications
            else {
                //LOG_INFO("Indications toggled ON");
                ble_data.pbIndicationsEnabled = true;
                status = sl_bt_gatt_set_characteristic_notification(ble_data.clientConnectionHandle, ble_data.pbCharacteristicHandle, gatt_indication);

                if (status != SL_STATUS_OK) {
                    LOG_ERROR("sl_bt_gatt_set_characteristic_notification 2");
                }
            }
        }
    }

    // handle pairing process
    if ((evt->data.evt_system_external_signal.extsignals == EVENT_PB0) && (ble_data.passkeyConfirm == true) && ble_data.pb0Pressed) {
        status = sl_bt_sm_passkey_confirm(ble_data.clientConnectionHandle, 1);
        //LOG_INFO("PASSKEY CONFIRM");

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_sm_passkey_confirm");
        }

        ble_data.passkeyConfirm = false;
    }

#endif

}

// handles soft timer events
void ble_system_soft_timer_event(sl_bt_msg_t* evt) {

#if DEVICE_IS_BLE_SERVER

    // every 1 second
    if (evt->data.evt_system_soft_timer.handle == LCD_HANDLE) {
        // LOG_INFO("Soft Timer 1");

        displayUpdate(); // prevent charge buildup on LCD
    }

    uint16_t charHandle;
    size_t bufferLength;
    uint8_t buffer[5];
    buffer[0] = 0; // set flags

    // every 50 ms
    if (evt->data.evt_system_soft_timer.handle == QUEUE_HANDLE) {
        // LOG_INFO("Soft Timer 2");

        /*
         * check the queue for pending indications
         *
         * if: there are queued indications AND no indication in flight
         * then: remove 1 indication from the tail of queue, send indication, set indication in flight
         */

         if ((num_queue_entries > 0) && !(ble_data.indicationInFlight)) {
             CORE_DECLARE_IRQ_STATE;
             CORE_ENTER_CRITICAL();
             read_queue (&charHandle, &bufferLength, buffer);
             CORE_EXIT_CRITICAL();

             status = sl_bt_gatt_server_send_indication(
                     ble_data.serverConnectionHandle,
                     charHandle, // characteristic from gatt_db.h
                     bufferLength, // value length
                     buffer // value
                     );

             if (status != SL_STATUS_OK) {
                 LOG_ERROR("QUEUE sl_bt_gatt_server_send_indication");
             }
             else {
                 ble_data.indicationInFlight = true;
             }

         }

    }

#else
    // every 1 second
    if (evt->data.evt_system_soft_timer.handle == LCD_HANDLE) {
        // LOG_INFO("Soft Timer 1");

        displayUpdate(); // prevent charge buildup on LCD
    }
#endif
}

/*
 * display passkey message on LCD
 *
 * evt = event that occurred
 */
void ble_sm_confirm_passkey_id(sl_bt_msg_t* evt) {
    //LOG_INFO("PASSKEY CONFIRM EVENT");
#if DEVICE_IS_BLE_SERVER
    if (ble_data.bonded == false) {
        displayPrintf(DISPLAY_ROW_PASSKEY, "Passkey %06d" , evt->data.evt_sm_passkey_display.passkey);
        displayPrintf(DISPLAY_ROW_ACTION, "Confirm with PB0");
        ble_data.passkeyConfirm = true;
    }
#else
    displayPrintf(DISPLAY_ROW_PASSKEY, "Passkey %06d" , evt->data.evt_sm_passkey_display.passkey);
    displayPrintf(DISPLAY_ROW_ACTION, "Confirm with PB0");
    ble_data.passkeyConfirm = true;
#endif
}

// displays bonding success message on LCD
void ble_sm_bonded_id() {
    //LOG_INFO("BONDED EVENT");
    ble_data.bonded = true;
    displayPrintf(DISPLAY_ROW_CONNECTION, "Bonded");
    displayPrintf(DISPLAY_ROW_PASSKEY, "");
    displayPrintf(DISPLAY_ROW_ACTION, "");
}

// displays bonding failure message on LCD
void ble_sm_bonding_failed_id() {
    //LOG_INFO("BONDING FAILED EVENT");
    displayPrintf(DISPLAY_ROW_CONNECTION, "Bonding Failed!");
    displayPrintf(DISPLAY_ROW_PASSKEY, "");
    displayPrintf(DISPLAY_ROW_ACTION, "");
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

    // temperature measurement indication handling
    if (characteristic == gattdb_temperature_measurement) {
        if (status_flags == gatt_server_client_config) {
            if (client_config_flags == gatt_disable) {
                ble_data.tempIndicationsEnabled = false;
                gpioLed0SetOff();
                displayPrintf(DISPLAY_ROW_TEMPVALUE, ""); // remove temp value from LCD
            }

            if (client_config_flags == gatt_indication) {
                gpioLed0SetOn();
                ble_data.tempIndicationsEnabled = true;
            }
        }

        if (status_flags == gatt_server_confirmation) {
            ble_data.indicationInFlight = false;
        }
    }

    // push button state indication handling
    if (characteristic == gattdb_button_state) {
        if (status_flags == gatt_server_client_config) {
            if (client_config_flags == gatt_disable) {
                ble_data.pbIndicationsEnabled = false;
                gpioLed1SetOff();
            }

            if (client_config_flags == gatt_indication) {
                gpioLed1SetOn();
                ble_data.pbIndicationsEnabled = true;
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

// accepts the bonding request
void ble_server_sm_confirm_bonding_event() {
    status = sl_bt_sm_bonding_confirm(ble_data.serverConnectionHandle, 1);

    if (status != SL_STATUS_OK) {
        LOG_ERROR("sl_bt_sm_bonding_confirm");
    }
}


// CLIENT EVENTS BELOW

// handles scan report logic, opens connection if server found
void ble_client_scanner_scan_report_event(sl_bt_msg_t* evt) {

    // needed variables for opening connection
    uint8_t connection;

    // check conditions for opening connection - bluetooth addresses match, packet and address types are expected values
    if (addressesMatch(evt->data.evt_scanner_scan_report.address, ble_data.serverAddress) &&
            ((evt->data.evt_scanner_scan_report.packet_type & 7) == 0) && // lower 3 bits -> corresponds to connectable scannable undirected advertising
            (evt->data.evt_scanner_scan_report.address_type == 0)) { // public address type

        status = sl_bt_scanner_stop();

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_scanner_stop");
        }

        status = sl_bt_connection_open(ble_data.serverAddress, evt->data.evt_scanner_scan_report.address_type, sl_bt_gap_1m_phy, &connection);

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_connection_open");
        }

    }

}

// notifies scheduler that a GATT procedure has been completed
void ble_client_gatt_procedure_completed_event(sl_bt_msg_t* evt) {

    //LOG_INFO("CLIENT: GATT PROCEDURE COMPLETED: %d", evt->data.evt_gatt_procedure_completed.result);
    //scheduler_set_client_event(EVENT_GATT_PROCEDURE_COMPLETED);

    status = evt->data.evt_gatt_procedure_completed.result;

    if (status != SL_STATUS_OK) {
        LOG_ERROR("GATT procedure completed error: %d", status);
    }

    if (status == 0x110F) { // insufficient encryption case, first time pressing PB1
        status = sl_bt_sm_increase_security(ble_data.clientConnectionHandle);

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_sm_increase_security: %d", status);
        }
    }

}

// saves the handle of the services
void ble_client_gatt_service_event(sl_bt_msg_t* evt) {

    //LOG_INFO("GATT Service Event ***");

    // check if match with HTM service
    if (memcmp(evt->data.evt_gatt_service.uuid.data, htm_service.data, htm_service.len) == 0) {
        ble_data.htmServiceHandle = evt->data.evt_gatt_service.service;
    }

    // check if match with PB service
    if (memcmp(evt->data.evt_gatt_service.uuid.data, pb_service.data, pb_service.len) == 0) {
        ble_data.pbServiceHandle = evt->data.evt_gatt_service.service;
    }
}

// saves the handle of the characteristics
void ble_client_gatt_characteristic_event(sl_bt_msg_t* evt) {

    //LOG_INFO("GATT Characteristic Event ***");

    // check if match with HTM characteristic
    if (memcmp(evt->data.evt_gatt_characteristic.uuid.data, htm_characteristic.data, htm_characteristic.len) == 0) {
        ble_data.htmCharacteristicHandle = evt->data.evt_gatt_characteristic.characteristic;
    }

    // check if match with PB characteristic
    if (memcmp(evt->data.evt_gatt_characteristic.uuid.data, pb_characteristic.data, pb_characteristic.len) == 0) {
        ble_data.pbCharacteristicHandle = evt->data.evt_gatt_characteristic.characteristic;
    }
}

// saves values from indication and read events, updates the LCD display based on them
void ble_client_gatt_characteristic_value_event(sl_bt_msg_t* evt) {

    // for all cases -  if char handle and opcode is expected, save value and send confirmation

    // HTM indications
    if ((evt->data.evt_gatt_characteristic_value.att_opcode == sl_bt_gatt_handle_value_indication) &&
            (evt->data.evt_gatt_characteristic_value.characteristic == ble_data.htmCharacteristicHandle)) {

        ble_data.htmCharacteristicValue.len = evt->data.evt_gatt_characteristic_value.value.len;

        // save value into data structure
        for (int i=0; i<ble_data.htmCharacteristicValue.len; i++) {
            ble_data.htmCharacteristicValue.data[i] = evt->data.evt_gatt_characteristic_value.value.data[i];
        }

        // convert value to int, save in data structure
        ble_data.tempMeasurement = FLOAT_TO_INT32(ble_data.htmCharacteristicValue.data);

        // display temp here, value received from server
        displayPrintf(DISPLAY_ROW_TEMPVALUE, "Temp=%d", ble_data.tempMeasurement);

        // send indication confirmation back to server
        sl_bt_gatt_send_characteristic_confirmation(ble_data.clientConnectionHandle);
    }

    // PB Indications
    if ((evt->data.evt_gatt_characteristic_value.att_opcode == sl_bt_gatt_handle_value_indication) &&
            (evt->data.evt_gatt_characteristic_value.characteristic == ble_data.pbCharacteristicHandle)) {

        //LOG_INFO("PB Opcode: %x", evt->data.evt_gatt_characteristic_value.att_opcode);

        ble_data.pbCharacteristicValue.len = evt->data.evt_gatt_characteristic_value.value.len; // 2 bytes

        //LOG_INFO("PB CHAR VAL LEN: %d", ble_data.pbCharacteristicValue.len);

        // save value into data structure
        for (int i=0; i<ble_data.pbCharacteristicValue.len; i++) {
            ble_data.pbCharacteristicValue.data[i] = evt->data.evt_gatt_characteristic_value.value.data[i];
            //LOG_INFO("Index %d: %d", i, ble_data.pbCharacteristicValue.data[i]);

        }

        // data format is:
        // [0] = flags byte = 0x00
        // [1] = button status = 0 (released) or 1 (pressed)

        if (ble_data.pbCharacteristicValue.data[1] == 0) {
            displayPrintf(DISPLAY_ROW_9, "Button Released");
            //LOG_INFO("BUTTON RELEASED");
        }

        if (ble_data.pbCharacteristicValue.data[1] == 1) {
            displayPrintf(DISPLAY_ROW_9, "Button Pressed");
            //LOG_INFO("BUTTON PRESSED");
        }

        // send indication confirmation back to server
        sl_bt_gatt_send_characteristic_confirmation(ble_data.clientConnectionHandle);

    }

    // PB Reads
    if ((evt->data.evt_gatt_characteristic_value.att_opcode == gatt_read_response) &&
            (evt->data.evt_gatt_characteristic_value.characteristic == ble_data.pbCharacteristicHandle)) {

        ble_data.pbCharacteristicValue.len = evt->data.evt_gatt_characteristic_value.value.len; // 1 byte

        ble_data.pbCharacteristicValue.data[0] = evt->data.evt_gatt_characteristic_value.value.data[0];

        // data format is:
        // [0] = button status = 0 (released) or 1 (pressed)

        if (ble_data.pbCharacteristicValue.data[0] == 0) {
            displayPrintf(DISPLAY_ROW_9, "Button Released");
        }

        if (ble_data.pbCharacteristicValue.data[0] == 1) {
            displayPrintf(DISPLAY_ROW_9, "Button Pressed");
        }

        ble_data.readInFlight = false;

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
            ble_external_signal_event(evt);
            break;

        case sl_bt_evt_system_soft_timer_id:
            ble_system_soft_timer_event(evt);
            break;

        case sl_bt_evt_sm_confirm_passkey_id:
            ble_sm_confirm_passkey_id(evt);
            break;

        case sl_bt_evt_sm_bonded_id:
            ble_sm_bonded_id();
            break;

        case sl_bt_evt_sm_bonding_failed_id:
            ble_sm_bonding_failed_id();
            break;

        // events just for servers
        case sl_bt_evt_gatt_server_characteristic_status_id:
            ble_server_characteristic_status_event(evt);
            break;

        case sl_bt_evt_gatt_server_indication_timeout_id:
            ble_server_indication_timeout_event();
            break;

        case sl_bt_evt_sm_confirm_bonding_id:
            ble_server_sm_confirm_bonding_event();
            break;

        // events just for clients
        case sl_bt_evt_scanner_scan_report_id:
            ble_client_scanner_scan_report_event(evt);
            break;

        case sl_bt_evt_gatt_procedure_completed_id:
            ble_client_gatt_procedure_completed_event(evt);
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

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


// provides access to bluetooth data structure for other .c files
ble_data_struct_t* get_ble_data_ptr() {
    return (&ble_data);
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
/*static bool addressesMatch(bd_addr a1, bd_addr a2) {

    return ((a1.addr[0] == a2.addr[0]) &&
            (a1.addr[1] == a2.addr[1]) &&
            (a1.addr[2] == a2.addr[2]) &&
            (a1.addr[3] == a2.addr[3]) &&
            (a1.addr[4] == a2.addr[4]) &&
            (a1.addr[5] == a2.addr[5]));
}*/

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

// called in state machine to send heart rate and blood oxygen data to client
void ble_transmit_heart_data() {

    uint8_t data1 = ble_data.heart_rate;
    uint8_t data2 = ble_data.blood_oxygen;

    //LOG_INFO("GATT WRITE: %d  %d", data1, data2);

    // write value to GATT database
    status = sl_bt_gatt_server_write_attribute_value(
            gattdb_heart_rate_measurement, // characteristic from gatt_db.h
            0, // offset
            1, // length
            &data1 // pointer to value
            );

    if (status != SL_STATUS_OK) {
        LOG_ERROR("HEART RATE sl_bt_gatt_server_write_attribute_value");
    }

    // write value to GATT database
    status = sl_bt_gatt_server_write_attribute_value(
            gattdb_blood_oxygen_measurement, // characteristic from gatt_db.h
            0, // offset
            1, // length
            &data2 // pointer to value
            );

    if (status != SL_STATUS_OK) {
        LOG_ERROR("BLOOD OXYGEN sl_bt_gatt_server_write_attribute_value");
    }

    uint8_t data_buffer[2];
    data_buffer[0] = 0; // flags byte
    data_buffer[1] = ble_data.heart_rate;

    if (ble_data.connectionOpen && ble_data.heartRateIndicationsEnabled && ble_data.bonded) {

        // no indication in flight, send right away
        if (!(ble_data.indicationInFlight)) {
            status = sl_bt_gatt_server_send_indication(
                    ble_data.serverConnectionHandle,
                    gattdb_heart_rate_measurement, // characteristic from gatt_db.h
                    2, // value length
                    data_buffer // value
                    );


            if (status != SL_STATUS_OK) {
                LOG_ERROR("HEART RATE sl_bt_gatt_server_send_indication");
            }
            else {
                ble_data.indicationInFlight = true;
            }
        }
        else { // put into circular buffer, send later
            //LOG_INFO("HR CIRC QUEUE");
            CORE_DECLARE_IRQ_STATE;
            CORE_ENTER_CRITICAL();
            write_queue(gattdb_heart_rate_measurement, 2, data_buffer);
            CORE_EXIT_CRITICAL();
        }
    }

    data_buffer[0] = 0; // flags byte
    data_buffer[1] = ble_data.blood_oxygen;

    if (ble_data.connectionOpen && ble_data.bloodOxygenIndicationsEnabled && ble_data.bonded) {

        // no indication in flight, send right away
        if (!(ble_data.indicationInFlight)) {
            status = sl_bt_gatt_server_send_indication(
                    ble_data.serverConnectionHandle,
                    gattdb_blood_oxygen_measurement, // characteristic from gatt_db.h
                    2, // value length
                    data_buffer // value
                    );


            if (status != SL_STATUS_OK) {
                LOG_ERROR("BLOOD OXYGEN sl_bt_gatt_server_send_indication");
            }
            else {
                ble_data.indicationInFlight = true;
            }
        }
        else { // put into circular buffer, send later
            //LOG_INFO("BLOOD OX CIRC QUEUE");
            CORE_DECLARE_IRQ_STATE;
            CORE_ENTER_CRITICAL();
            write_queue(gattdb_blood_oxygen_measurement, 2, data_buffer);
            CORE_EXIT_CRITICAL();
        }
    }

}

// COMMON SERVER + CLIENT EVENTS BELOW

// This event indicates the device has started and the radio is ready
void ble_boot_event() {

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
    displayPrintf(DISPLAY_ROW_ASSIGNMENT, "Final Project");

    displayPrintf(DISPLAY_ROW_CONNECTION, "Advertising");

    displayPrintf(DISPLAY_ROW_BTADDR, "%x:%x:%x:%x:%x:%x",
                  ble_data.myAddress.addr[0], ble_data.myAddress.addr[1], ble_data.myAddress.addr[2],
                  ble_data.myAddress.addr[3], ble_data.myAddress.addr[4], ble_data.myAddress.addr[5]);

    displayPrintf(DISPLAY_ROW_ACTION, "Place Finger!");

}

/*
 * This event indicates that a new connection was opened
 *
 * evt = event that occurred
 */
void ble_connection_opened_event(sl_bt_msg_t* evt) {

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

    uint16_t min_interval = 60; // Value = Time in ms / 1.25 ms = 75 / 1.25 = 60
    uint16_t max_interval = 60; // same math
    uint16_t latency = 3; // how many connection intervals the slave can skip - off air for up to 300 ms

    // value greater than (1 + slave latency) * (connection_interval * 2) = (1 + 3) * (75 * 2) = 4 * 150 = 600 ms
    // Value = Time / 10 ms = 600 / 10 = 60 -> use 80
    uint16_t timeout = 80;

    uint16_t min_ce_length = 0; // default
    uint16_t max_ce_length = 0xffff; // no limitation

    // Send a request with a set of parameters to the master
    status = sl_bt_connection_set_parameters(ble_data.serverConnectionHandle,
                                             min_interval,
                                             max_interval,
                                             latency,
                                             timeout,
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

}

// This event indicates that a connection was closed
void ble_connection_closed_event() {

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

    //LOG_INFO("EXTERNAL SIGNAL EVENT");

    // handle pairing process
    if ((evt->data.evt_system_external_signal.extsignals == EVENT_PB0) && (ble_data.passkeyConfirm == true) && ble_data.pb0Pressed) {

        status = sl_bt_sm_passkey_confirm(ble_data.serverConnectionHandle, 1);

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_sm_passkey_confirm");
        }
        ble_data.passkeyConfirm = false;
    }

    // var to save bool as int
    uint8_t button_state;

    // whenever a change in button state occurs (pressed or released), update the GATT database
    if ((evt->data.evt_system_external_signal.extsignals == EVENT_PB0)) {

        // set button_state variable, update the LCD
        if (ble_data.pb0Pressed) {
            button_state = 1;
            //displayPrintf(DISPLAY_ROW_9, "Button Pressed");
        }
        else {
            button_state = 0;
            //displayPrintf(DISPLAY_ROW_9, "Button Released");
        }

        // write command to GATT database
        // parameters: attribute from gatt_db.h, value offset, array length, value
        status = sl_bt_gatt_server_write_attribute_value(gattdb_button_state, 0, 1, &button_state);

        if (status != SL_STATUS_OK) {
            LOG_ERROR("sl_bt_gatt_server_write_attribute_value");
        }

        // send an indication
        ble_transmit_button_state();
    }


}

// handles soft timer events
void ble_system_soft_timer_event(sl_bt_msg_t* evt) {

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

}

/*
 * display passkey message on LCD
 *
 * evt = event that occurred
 */
void ble_sm_confirm_passkey_id(sl_bt_msg_t* evt) {
    //LOG_INFO("PASSKEY CONFIRM EVENT");
    if (ble_data.bonded == false) {
        displayPrintf(DISPLAY_ROW_PASSKEY, "Passkey %06d" , evt->data.evt_sm_passkey_display.passkey);
        displayPrintf(DISPLAY_ROW_ACTION, "Confirm with PB0");
        ble_data.passkeyConfirm = true;
    }
}

// displays bonding success message on LCD
void ble_sm_bonded_id() {
    //LOG_INFO("BONDED EVENT");
    ble_data.bonded = true;
    displayPrintf(DISPLAY_ROW_CONNECTION, "Bonded");
    displayPrintf(DISPLAY_ROW_PASSKEY, "");
    displayPrintf(DISPLAY_ROW_ACTION, "Place Finger!");
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

    // push button state indication handling
    if (characteristic == gattdb_button_state) {
        if (status_flags == gatt_server_client_config) {
            if (client_config_flags == gatt_disable) {
                ble_data.pbIndicationsEnabled = false;
            }

            if (client_config_flags == gatt_indication) {
                ble_data.pbIndicationsEnabled = true;
            }
        }

        if (status_flags == gatt_server_confirmation) {
            ble_data.indicationInFlight = false;
        }
    }

    // heart rate indication handling
    if (characteristic == gattdb_heart_rate_measurement) {
        if (status_flags == gatt_server_client_config) {
            if (client_config_flags == gatt_disable) {
                ble_data.heartRateIndicationsEnabled = false;
                gpioLed1SetOff();
            }

            if (client_config_flags == gatt_indication) {
                gpioLed1SetOn();
                ble_data.heartRateIndicationsEnabled = true;
            }
        }

        if (status_flags == gatt_server_confirmation) {
            ble_data.indicationInFlight = false;
        }
    }

    // blood oxygen indication handling
    if (characteristic == gattdb_blood_oxygen_measurement) {
        if (status_flags == gatt_server_client_config) {
            if (client_config_flags == gatt_disable) {
                ble_data.bloodOxygenIndicationsEnabled = false;
                //gpioLed0SetOff();
            }

            if (client_config_flags == gatt_indication) {
                //gpioLed0SetOn();
                ble_data.bloodOxygenIndicationsEnabled = true;
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

    }
}

/*
 * scheduler.h
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_SCHEDULER_H_
#define SRC_SCHEDULER_H_

#include "stdint.h"
#include "ble.h"

typedef enum {
    EVENT_IDLE,
    EVENT_MEASURE_TEMP,
    EVENT_TIMER_EXPIRED,
    EVENT_I2C_DONE,
    EVENT_PB0,
    EVENT_PB1
} server_events_t;

typedef enum {
    STATE_IDLE,
    STATE_SENSOR_POWERUP,
    STATE_I2C_WRITE,
    STATE_INTERIM_DELAY,
    STATE_I2C_READ,
} server_states_t;

typedef enum {
    STATE_AWAITING_CONNECTION,
    STATE_HTM_SERVICE_DISCOVERY,
    STATE_HTM_CHARACTERISTIC_DISCOVERY,
    STATE_HTM_ENABLING_INDICATIONS,
    STATE_PB_SERVICE_DISCOVERY,
    STATE_PB_CHARACTERISTIC_DISCOVERY,
    STATE_PB_ENABLING_INDICATIONS,
    STATE_DISCOVERED,
} client_states_t;

void init_scheduler();

void scheduler_set_event_UF();

void scheduler_set_event_COMP1();

void scheduler_set_event_I2C();

void scheduler_set_event_PB0_pressed();

void scheduler_set_event_PB0_released();

void scheduler_set_event_PB1_pressed();

void scheduler_set_event_PB1_released();

uint8_t bluetooth_connection_errors();

uint8_t external_signal_event_match(sl_bt_msg_t* evt, uint8_t event_id);

void temperature_state_machine(sl_bt_msg_t* evt);

void discovery_state_machine(sl_bt_msg_t* evt);

#endif /* SRC_SCHEDULER_H_ */

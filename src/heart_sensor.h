/*
 * heart_sensor.h
 *
 *  Created on: Apr 17, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_HEART_SENSOR_H_
#define SRC_HEART_SENSOR_H_

#include "stdint.h"

//#define LOW_POWER_MODE 0


#define FINGER_DETECTED 3
#define OBJECT_DETECTED 1
#define NOTHING_DETECTED 0
#define CONFIDENCE_THRESHOLD 93

typedef struct heart_sensor_data {
    uint16_t heart_rate;
    uint16_t blood_oxygen;
    uint16_t confidence;
    uint16_t finger_status;
} heart_sensor_data;

heart_sensor_data* get_heart_data_ptr();

void init_heart_sensor();

void turn_off_heart_sensor();

void turn_on_heart_sensor();

void read_heart_sensor();

#endif /* SRC_HEART_SENSOR_H_ */

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

// structure for saving BMP mode 1 sensor data
typedef struct heart_sensor_data {
    uint16_t heart_rate;
    uint16_t blood_oxygen;
    uint16_t confidence;
    uint16_t finger_status;
} heart_sensor_data;


heart_sensor_data* get_heart_data_ptr();

void disable_reset();
void enable_reset();
void disable_mfio();
void enable_mfio();
void set_mfio_interrupt();

void read_device_mode();
void read_sensor_hub_version();
void set_output_mode();
void set_fifo_threshold();
void agc_algo_control();
void enable_sensor();
void enable_algo();
void read_algo_samples();
void read_sensor_hub_status();
void num_samples_out_fifo();
void read_fill_array();
void disable_sensor();
void disable_algo();

void process_raw_heart_data();
void read_heart_sensor();
void turn_off_heart_sensor();
void turn_on_heart_sensor();
void init_heart_sensor();

#endif /* SRC_HEART_SENSOR_H_ */

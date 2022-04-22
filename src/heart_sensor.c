/*
 * heart_sensor.c
 *
 *  Created on: Apr 17, 2022
 *      Author: bjornnelson
 */


#include "heart_sensor.h"
#include "timers.h"
#include "gpio.h"
#include "i2c.h"

#define INCLUDE_LOG_DEBUG 1
#include "log.h"

#define CMD_DELAY 60

uint8_t read_device_mode_cmd[2] = {0x02, 0x00};
uint8_t read_sensor_hub_version_cmd[2] = {0xFF, 0x03};
uint8_t set_output_mode_cmd[3] = {0x10, 0x00, 0x02}; // algorithm data
uint8_t set_fifo_threshold_cmd[3] = {0x10, 0x01, 0x01}; // last byte = interrupt threshold for FIFO almost full
uint8_t agc_algo_control_cmd[3] = {0x52, 0x00, 0x01}; // enable the AGC algorithm
uint8_t max30101_control_cmd[3] = {0x44, 0x03, 0x01}; // enable the MAX 30001 sensor
uint8_t maxim_fast_algo_control_cmd[3] = {0x52, 0x02, 0x01}; // enable the WHRM, MaximFast algorithm
uint8_t read_algo_samples_cmd[3] = {0x51, 0x00, 0x03}; // read number of samples being averaged in algorithm
uint8_t read_sensor_hub_status_cmd[2] = {0x00, 0x00};
uint8_t num_samples_out_fifo_cmd[2] = {0x12, 0x00}; // get the number of samples available in the fifo
uint8_t read_fill_array_cmd[2] = {0x12, 0x01}; //

typedef struct heart_sensor_data {
    uint16_t heart_rate;
    uint16_t blood_oxygen;
    uint16_t confidence;
    uint16_t finger_status;
} heart_sensor_data;

heart_sensor_data health_data;

#define MAXFAST_ARRAY_SIZE        7
uint8_t bpm_arr[MAXFAST_ARRAY_SIZE];

void disable_reset() {
    GPIO_PinOutClear(MAX30101_PORT, MAX30101_RESET_PIN);
}

void enable_reset() {
    GPIO_PinOutSet(MAX30101_PORT, MAX30101_RESET_PIN);
}

void disable_mfio() {
    GPIO_PinOutClear(MAX30101_PORT, MAX30101_MFIO_PIN);
}

void enable_mfio() {
    GPIO_PinOutSet(MAX30101_PORT, MAX30101_MFIO_PIN);
}

void set_mfio_interrupt() {
    GPIO_PinModeSet(MAX30101_PORT, MAX30101_MFIO_PIN, gpioModeInputPull, true);
    GPIO_ExtIntConfig (MAX30101_PORT, MAX30101_MFIO_PIN, MAX30101_MFIO_PIN, false, true, true); // interrupts enabled after config complete, on falling edge
}

void read_device_mode() {

    // write family byte and index byte
    i2c_write(read_device_mode_cmd, sizeof(read_device_mode_cmd));

    // delay 60 microseconds
    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("Could not communicate with sensor");
    else
        LOG_INFO("Sensor started");
}

void read_sensor_hub_version() {
    i2c_write(read_sensor_hub_version_cmd, sizeof(read_sensor_hub_version_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("read_sensor_hub_version()");
}

void set_output_mode() {

    i2c_write(set_output_mode_cmd, sizeof(set_output_mode_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("set_output_mode()");
}

void set_fifo_threshold() {
    i2c_write(set_fifo_threshold_cmd, sizeof(set_fifo_threshold_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("set_fifo_threshold()");
}

void agc_algo_control() {
    i2c_write(agc_algo_control_cmd, sizeof(agc_algo_control_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("agc_algo_control()");
}

void max_30101_control() {
    i2c_write(max30101_control_cmd, sizeof(max30101_control_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("max_30101_control()");
}

void maxim_fast_algo_control() {
    i2c_write(maxim_fast_algo_control_cmd, sizeof(maxim_fast_algo_control_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("maxim_fast_algo_control()");
}

void read_algo_samples() {
    i2c_write(read_algo_samples_cmd, sizeof(read_algo_samples_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("read_algo_samples()");
}

void read_sensor_hub_status() {

    uint8_t buf[8];

    i2c_write(read_sensor_hub_status_cmd, sizeof(read_sensor_hub_status_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read_addr(buf, 8);

    if (buf[0] != 0)
        LOG_ERROR("read_sensor_hub_status()");

    if (buf[1] == 1) {
        LOG_ERROR("Sensor communication error");
        health_data.heart_rate = 0;
        health_data.blood_oxygen = 0;
        health_data.confidence = 0;
    }

    LOG_INFO("DataRdyInt = %d (0=no, 1=yes)", buf[3]);
}

void num_samples_out_fifo() {

    uint8_t buf[8];

    i2c_write(num_samples_out_fifo_cmd, sizeof(num_samples_out_fifo_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read_addr(buf, 8);

    if (buf[0] != 0)
        LOG_ERROR("num_samples_out_fifo()");

    LOG_INFO("Num Samples in fifo: %d", buf[1]);
}

void read_fill_array() {
    // family byte - 0x12
    // index byte - 0x01
    // number of reads
    // array - bpm_arr

    //for (int i=0; i<MAXFAST_ARRAY_SIZE; i++) {
        i2c_write(read_fill_array_cmd, sizeof(read_fill_array_cmd));

        timer_wait_us_polled(CMD_DELAY);

        i2c_read_addr(bpm_arr, MAXFAST_ARRAY_SIZE);
    //}

}

void process_raw_heart_data() {
    /*
     * bytes 0-1: Heart Rate (bpm): 16-bit, LSB = 0.1 bpm
     * byte 2: Confidence level (0 - 100%): 8-bit, LSB = 1%
     * bytes 3-4: SpO 2 value (0 - 100%): 16-bit, LSB = 0.1%
     * byte 5: MaximFast State Machine Status Codes
     *         0 = no object detected
     *         1 = object detected
     *         2 = object other than finger detected
     *         3 = finger detected
     *
     * END OF DATA
     */


    for (int i=0; i<7; i++) {
        LOG_INFO("Byte %d: %d", i, bpm_arr[i]);
    }

    health_data.heart_rate = bpm_arr[1] << 8;
    health_data.heart_rate |= bpm_arr[2];
    health_data.heart_rate /= 10;

    health_data.blood_oxygen = bpm_arr[4] << 8;
    health_data.blood_oxygen |= bpm_arr[5];
    health_data.blood_oxygen /= 10;

    health_data.confidence = bpm_arr[3];

    health_data.finger_status = bpm_arr[6];

    LOG_INFO("** RESULTS **");
    LOG_INFO("Heart Rate: %d   Blood Oxygen: %d    Confidence: %d    Status: %d\n", health_data.heart_rate, health_data.blood_oxygen, health_data.confidence, health_data.finger_status);

}

// like readBPM in Arduino code
void read_heart_sensor() {



    read_sensor_hub_status();
    //LOG_INFO("Read sensor hub status");
    //print_heart_data();

    // CAREFUL, THIS OVERWRITES PART OF HEART RATE DATA
    num_samples_out_fifo();
    //LOG_INFO("Num samples out fifo");
    //print_heart_data();

    read_fill_array();

    process_raw_heart_data();

}


void init_heart_sensor() {
    LOG_INFO("Initializing heart sensor");

    // GPIO pin modes already configured

    //LOG_INFO("2 sec delay");
    //timer_wait_us_polled(2000000);

    init_i2c();

    disable_reset();
    enable_mfio();
    timer_wait_us_polled(10000); // delay 10 ms, maybe unnecessary

    enable_reset();
    timer_wait_us_polled(1000000); // delay 100 ms, 50 ms minimum

    // sensor now in application mode

    set_mfio_interrupt();


    LOG_INFO("Initial data");
    reset_heart_data();
    print_heart_data();

    LOG_INFO("Read device mode");
    read_device_mode();
    print_heart_data();


    /*LOG_INFO("Read sensor hub version");
    read_sensor_hub_version();
    print_heart_data();*/

    LOG_INFO("Set output mode");
    set_output_mode();
    print_heart_data();

    LOG_INFO("Set fifo threshold");
    set_fifo_threshold();
    print_heart_data();


    LOG_INFO("AGC algo control");
    agc_algo_control();
    print_heart_data();

    LOG_INFO("MAX 30101 control");
    max_30101_control();
    print_heart_data();

    LOG_INFO("Maxim fast algo control");
    maxim_fast_algo_control();
    print_heart_data();

    // readAlgoSamples
    LOG_INFO("Read algo samples");
    read_algo_samples();
    print_heart_data();

    // delay 1 sec
    LOG_INFO("Loading up the buffer with data - delay 4 seconds");
    timer_wait_us_polled(4000000000);

    LOG_INFO("Finished heart sensor initialization");


    while(1) {
        read_heart_sensor();
        timer_wait_us_polled(1000000);
    }

}

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

// 60 us too short for some functions, experimented with values
#define CMD_DELAY 50000

#define DATASHEET_CMD_DELAY 60

uint8_t read_device_mode_cmd[2] = {0x02, 0x00};
uint8_t read_sensor_hub_version_cmd[2] = {0xFF, 0x03}; // for debugging

uint8_t set_output_mode_cmd[3] = {0x10, 0x00, 0x02}; // mode = algorithm data
uint8_t set_fifo_threshold_cmd[3] = {0x10, 0x01, 0x01}; // last byte = interrupt threshold for FIFO almost full
uint8_t agc_algo_control_cmd[3] = {0x52, 0x00, 0x01}; // enable the AGC algorithm
uint8_t enable_sensor_cmd[3] = {0x44, 0x03, 0x01}; // turn on the MAX 30001 sensor
uint8_t enable_algo_cmd[3] = {0x52, 0x02, 0x01}; // enable the WHRM, MaximFast algorithm
uint8_t read_algo_samples_cmd[3] = {0x51, 0x00, 0x03}; // read number of samples being averaged in algorithm -> 5

uint8_t read_sensor_hub_status_cmd[2] = {0x00, 0x00};
uint8_t num_samples_out_fifo_cmd[2] = {0x12, 0x00}; // get the number of samples available in the fifo
uint8_t read_fill_array_cmd[2] = {0x12, 0x01}; // get the data

uint8_t disable_sensor_cmd[3] = {0x44, 0x03, 0x00}; // turn off the MAX 30001 sensor
uint8_t disable_algo_cmd[3] = {0x52, 0x02, 0x00}; // turn off the WHRM, MaximFast algorithm

// structure for saving sensor data
heart_sensor_data health_data;

// bytes returned by read_fill_array()
#define MAXFAST_ARRAY_SIZE        7
uint8_t bpm_arr[MAXFAST_ARRAY_SIZE];

// returns ptr to data read
heart_sensor_data* get_heart_data_ptr() {
    return &health_data;
}

// sets reset pin low
void disable_reset() {
    GPIO_PinOutClear(MAX30101_PORT, MAX30101_RESET_PIN);
}

// sets reset pin high
void enable_reset() {
    GPIO_PinOutSet(MAX30101_PORT, MAX30101_RESET_PIN);
}

// sets MFIO pin low
void disable_mfio() {
    GPIO_PinOutClear(MAX30101_PORT, MAX30101_MFIO_PIN);
}

// sets MFIO pin high
void enable_mfio() {
    GPIO_PinOutSet(MAX30101_PORT, MAX30101_MFIO_PIN);
}

// enables MFIO pin interrupts
void set_mfio_interrupt() {
    GPIO_PinModeSet(MAX30101_PORT, MAX30101_MFIO_PIN, gpioModeInputPull, true);
    GPIO_ExtIntConfig (MAX30101_PORT, MAX30101_MFIO_PIN, MAX30101_MFIO_PIN, false, true, true); // interrupts enabled after config complete, on falling edge
}

// get device mode information
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

// get device version information
void read_sensor_hub_version() {
    i2c_write(read_sensor_hub_version_cmd, sizeof(read_sensor_hub_version_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("read_sensor_hub_version()");
}

// configure the device for algorithm data output mode
void set_output_mode() {

    i2c_write(set_output_mode_cmd, sizeof(set_output_mode_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("set_output_mode()");
}

// set the fifo threshold to 1
void set_fifo_threshold() {
    i2c_write(set_fifo_threshold_cmd, sizeof(set_fifo_threshold_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("set_fifo_threshold()");
}

// enable the AGC algorithm
void agc_algo_control() {
    i2c_write(agc_algo_control_cmd, sizeof(agc_algo_control_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("agc_algo_control()");
}

// turns on the sensor
void enable_sensor() {
    i2c_write(enable_sensor_cmd, sizeof(enable_sensor_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("enable_sensor()");
}

// turns on the data processing algorithm
void enable_algo() {
    i2c_write(enable_algo_cmd, sizeof(enable_algo_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("enable_algo()");
}

// get the number of samples being averaged
void read_algo_samples() {
    i2c_write(read_algo_samples_cmd, sizeof(read_algo_samples_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read();

    if (status_byte_error())
        LOG_ERROR("read_algo_samples()");
}

// read the status of the sensor
void read_sensor_hub_status() {

    uint8_t buf[8];

    i2c_write(read_sensor_hub_status_cmd, sizeof(read_sensor_hub_status_cmd));

    timer_wait_us_polled(DATASHEET_CMD_DELAY);

    i2c_read_addr(buf, 8);

    if (buf[0] != 0)
        LOG_ERROR("read_sensor_hub_status()");

    if (buf[1] == 1) {
        LOG_ERROR("Sensor communication error");
        health_data.heart_rate = 0;
        health_data.blood_oxygen = 0;
        health_data.confidence = 0;
    }

    //LOG_INFO("DataRdyInt = %d (0=no, 1=yes)", buf[3]);
}

// get the number of samples in the fifo
void num_samples_out_fifo() {

    uint8_t buf[8];

    i2c_write(num_samples_out_fifo_cmd, sizeof(num_samples_out_fifo_cmd));

    timer_wait_us_polled(DATASHEET_CMD_DELAY);

    i2c_read_addr(buf, 8);

    if (buf[0] != 0)
        LOG_ERROR("num_samples_out_fifo()");

    //LOG_INFO("Num Samples in fifo: %d", buf[1]);
}

// do a reading
void read_fill_array() {

    i2c_write(read_fill_array_cmd, sizeof(read_fill_array_cmd));

    timer_wait_us_polled(DATASHEET_CMD_DELAY);

    i2c_read_addr(bpm_arr, MAXFAST_ARRAY_SIZE);

    if (bpm_arr[0] != 0)
        LOG_ERROR("read_fill_array()");

}

// turn off the sensor
void disable_sensor() {
    uint8_t buf[8];

    i2c_write(disable_sensor_cmd, sizeof(disable_sensor_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read_addr(buf, 8);

    if (buf[0] != 0)
        LOG_ERROR("disable_sensor()");

}

// turn off the data processing algorithm
void disable_algo() {
    uint8_t buf[8];

    i2c_write(disable_algo_cmd, sizeof(disable_algo_cmd));

    timer_wait_us_polled(CMD_DELAY);

    i2c_read_addr(buf, 8);

    if (buf[0] != 0)
        LOG_ERROR("disable_algo()");
}


// convert bytes returned into heart rate, blood oxygen, finger status, and confidence values
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
     */

    // shift bits and divide by 10
    health_data.heart_rate = bpm_arr[1] << 8;
    health_data.heart_rate |= bpm_arr[2];
    health_data.heart_rate /= 10;

    // shift bits and divide by 10
    health_data.blood_oxygen = bpm_arr[4] << 8;
    health_data.blood_oxygen |= bpm_arr[5];
    health_data.blood_oxygen /= 10;

    health_data.confidence = bpm_arr[3];

    health_data.finger_status = bpm_arr[6];
}

// perform command sequence for doing a reading
void read_heart_sensor() {

    //LOG_INFO("** READING HEART SENSOR **");

    // EM <= 1 required during I2C transfers
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

    read_sensor_hub_status();
    //LOG_INFO("Read sensor hub status");
    //print_heart_data();

    num_samples_out_fifo();
    //LOG_INFO("Num samples out fifo");
    //print_heart_data();

    read_fill_array();

    sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);

    process_raw_heart_data();

    // log captured data for debugging purposes
    LOG_INFO("Heart Rate: %d   Blood Oxygen: %d    Confidence: %d    Status: %d\n", health_data.heart_rate, health_data.blood_oxygen, health_data.confidence, health_data.finger_status);

}

// helper function for power savings
void turn_off_heart_sensor() {
    LOG_INFO("Turning off heart sensor\n");

    disable_sensor();
    disable_algo();
}

// helper function for power savings
void turn_on_heart_sensor() {
    LOG_INFO("Turning on heart sensor");

    enable_sensor();
    enable_algo();
}

// setup command sequence to get heart sensor ready for readings
void init_heart_sensor() {
    LOG_INFO("Initializing heart sensor");

    // GPIO pin modes already configured

    // EM <= 1 required during I2C transfers
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

    disable_reset();
    enable_mfio();
    timer_wait_us_polled(10000); // delay 10 ms, maybe unnecessary

    enable_reset();
    timer_wait_us_polled(1000000); // delay 1 s, 50 ms minimum

    // sensor now in application mode

    set_mfio_interrupt();

    //LOG_INFO("Read device mode");
    read_device_mode();

    /*LOG_INFO("Read sensor hub version");
    read_sensor_hub_version();
    print_heart_data();*/

    //LOG_INFO("Set output mode");
    set_output_mode();

    //LOG_INFO("Set fifo threshold");
    set_fifo_threshold();

    //LOG_INFO("AGC algo control");
    agc_algo_control();

    //LOG_INFO("MAX 30101 control");
    enable_sensor();

    //LOG_INFO("Maxim fast algo control");
    enable_algo();

    //LOG_INFO("Read algo samples");
    read_algo_samples();

    // drop pack down to EM2
    sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);

    LOG_INFO("Finished heart sensor initialization");

}

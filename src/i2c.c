/*
 * i2c.c
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#include "i2c.h"
#include "gpio.h"
#include "timers.h"

#include "em_cmu.h"
#include "sl_i2cspm.h"

// i2c device address
#define SI7021_ADDR 0x40

#define MAX30101_ADDR 0xAA // 0x55 << 1

// command id for measuring temperature in no hold master mode
#define MEASURE_TEMP_CMD 0xF3

// enable logging for temperature results to be displayed in console
#define INCLUDE_LOG_DEBUG 1
#include "log.h"

I2C_TransferSeq_TypeDef transfer_sequence;

uint8_t cmd_data[1];
uint8_t cmd_data_len;

uint8_t read_data[2];
uint8_t read_data_len;


// heart sensor variables
uint8_t heart_data[8];


uint8_t* get_heart_data() {
    return heart_data;
}

void reset_heart_data() {
    for (int i=0; i<8; i++) {
        heart_data[i] = 0;
    }
}

void print_heart_data() {
    LOG_INFO("** I2C READ DATA **");

    for (int i=0; i<8; i++) {
        LOG_INFO("Byte %d: %d", i, heart_data[i]);
    }
}

// returns 1 if there was an error, 0 if not
int status_byte_error() {
    return (heart_data[0] != 0x00);
}


// data structure for initializing i2c
static I2CSPM_Init_TypeDef i2c_settings = {
        .port = I2C0,
        .sclPort = gpioPortC,
        .sclPin = 10,
        .sdaPort = gpioPortC,
        .sdaPin = 11,
        .portLocationScl = 14,
        .portLocationSda = 16,
        .i2cRefFreq = 0,
        .i2cMaxFreq = I2C_FREQ_STANDARD_MAX,
        .i2cClhr = i2cClockHLRStandard
};


// calls the API's i2c setup function with i2c settings in typedef
// handles load power management initialization
void init_i2c() {

    // call the library setup function
    I2CSPM_Init(&i2c_settings);

}

void i2c_write(uint8_t* cmd, int len) {

    //init_i2c();

    // set i2c address and mode
    transfer_sequence.addr = MAX30101_ADDR;
    transfer_sequence.flags = I2C_FLAG_WRITE;
    transfer_sequence.buf[0].data = cmd;
    transfer_sequence.buf[0].len = len;

    // start the transfer
    I2C_TransferReturn_TypeDef transfer_status = I2CSPM_Transfer(I2C0, &transfer_sequence);

    // check for errors
    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }

}

void i2c_read() {

    //init_i2c();

    // set i2c address and mode
    transfer_sequence.addr = MAX30101_ADDR;
    transfer_sequence.flags = I2C_FLAG_READ;
    transfer_sequence.buf[0].data = heart_data;
    transfer_sequence.buf[0].len = sizeof(heart_data);

    // start the transfer
    I2C_TransferReturn_TypeDef transfer_status = I2CSPM_Transfer(I2C0, &transfer_sequence);

    // check for errors
    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }

}

void i2c_read_addr(uint8_t* save_addr, uint8_t num_bytes) {

    //init_i2c();

    // set i2c address and mode
    transfer_sequence.addr = MAX30101_ADDR;
    transfer_sequence.flags = I2C_FLAG_READ;
    transfer_sequence.buf[0].data = save_addr;
    transfer_sequence.buf[0].len = num_bytes;

    // start the transfer
    I2C_TransferReturn_TypeDef transfer_status = I2CSPM_Transfer(I2C0, &transfer_sequence);

    // check for errors
    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }

}

// load power management deinitialization
// called at the end of a temperature reading
void deinit_i2c() {

    // disable GPIOs for i2C
    gpioI2cSclDisable();
    gpioI2cSdaDisable();

    // disable control module
    I2C_Reset(I2C0);
    I2C_Enable(I2C0, false);

    // turn off clock to module
    CMU_ClockEnable(cmuClock_I2C0, false);
}

// performs an i2c write, to request a temperature measurement
void i2c_send_command() {

    // enable i2c interrupts
    NVIC_EnableIRQ(I2C0_IRQn);

    // set i2c address and mode
    transfer_sequence.addr = SI7021_ADDR << 1;
    transfer_sequence.flags = I2C_FLAG_WRITE;

    cmd_data[0] = MEASURE_TEMP_CMD; // command id
    cmd_data_len = 1; // transmit 1 byte

    // only buf[0] used in write mode
    transfer_sequence.buf[0].data = cmd_data;
    transfer_sequence.buf[0].len = cmd_data_len;

    // start the transfer
    I2C_TransferReturn_TypeDef transfer_status = I2C_TransferInit(I2C0, &transfer_sequence);

    // check for errors
    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }


}

// performs an i2c read, temperature measurement will be saved into read_data
void i2c_receive_data() {

    // enable i2c interrupts
    NVIC_EnableIRQ(I2C0_IRQn);

    // set i2c address and mode
    transfer_sequence.addr = SI7021_ADDR << 1;
    transfer_sequence.flags = I2C_FLAG_READ;

    read_data_len = 2; // receive 2 bytes

    // only buf[0] used in read mode
    transfer_sequence.buf[0].data = read_data;
    transfer_sequence.buf[0].len = read_data_len;

    // start the transfer
    I2C_TransferReturn_TypeDef transfer_status = I2C_TransferInit(I2C0, &transfer_sequence);

    // check for errors
    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }

}

/*
 * computes the most recently saved temperature measurement
 *
 * returns: temperature value in degrees C
 */
uint16_t get_temp() {
    uint16_t temp_data = ((read_data[0] << 8) | read_data[1]);
    float temp_celcius = ((175.72 * temp_data) / 65536) - 46.85;
    uint16_t temp_celcius_int = (uint16_t) temp_celcius;

    //LOG_INFO("Temperature: %d C", temp_celcius_int);

    return temp_celcius_int;
}

/*
 * decodes the return value from a I2C_Transfer() call
 * Logs relevant info to the console
 *
 * ret_value = returned value from I2C_Transfer()
 */
void process_i2c_status(I2C_TransferReturn_TypeDef ret_value) {
    switch(ret_value) {
        case i2cTransferInProgress:
            LOG_WARN("I2C Status: IN PROGRESS");
            break;
        case i2cTransferDone:
            LOG_INFO("I2C Status: DONE");
            break;
        case i2cTransferNack:
            LOG_WARN("I2C Status): NACK");
            break;
        case i2cTransferBusErr:
            LOG_WARN("I2C Status: BUS ERR");
            break;
        case i2cTransferArbLost:
            LOG_WARN("I2C Status: ARB LOST");
            break;
        case i2cTransferUsageFault:
            LOG_WARN("I2C Status: USAGE FAULT");
            break;
        case i2cTransferSwFault:
            LOG_WARN("I2C Status: SW FAULT");
            break;
        default:
            LOG_ERROR("Unknown I2C Status");
            break;

    }
}

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
static const uint8_t SI7021_ADDR = 0x40;

// command id for measuring temperature in no hold master mode
static const uint8_t MEASURE_TEMP_CMD = 0xF3;

// enable logging for temperature results to be displayed in console
#define INCLUDE_LOG_DEBUG 1
#include "log.h"

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

I2C_TransferSeq_TypeDef transfer_sequence;

uint8_t cmd_data;
uint8_t cmd_data_len;

uint16_t read_data;
uint8_t read_data_len;

// calls the API's i2c setup function with i2c settings in typedef
// handles load power management initialization
void init_i2c() {

    // call the library setup function
    I2CSPM_Init(&i2c_settings);

}

// load power management de-initialization
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

void i2c_send_command() {
    init_i2c();

    // enable i2c interrupts
    NVIC_EnableIRQ(I2C0_IRQn);

    transfer_sequence.addr = SI7021_ADDR << 1;
    transfer_sequence.flags = I2C_FLAG_WRITE;

    cmd_data = MEASURE_TEMP_CMD;
    cmd_data_len = 1;

    transfer_sequence.buf[0].data = &cmd_data;
    transfer_sequence.buf[0].len = cmd_data_len;

    I2C_TransferReturn_TypeDef transfer_status = I2C_TransferInit(I2C0, &transfer_sequence);

    // check for errors
    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }


}

void i2c_receive_data() {

    // enable i2c interrupts
    NVIC_EnableIRQ(I2C0_IRQn);

    transfer_sequence.addr = SI7021_ADDR << 1;
    transfer_sequence.flags = I2C_FLAG_READ;

    read_data_len = 2;

    transfer_sequence.buf[0].data = (uint8_t*)(&read_data);
    transfer_sequence.buf[0].len = read_data_len;

    I2C_TransferReturn_TypeDef transfer_status = I2C_TransferInit(I2C0, &transfer_sequence);

    // check for errors
    if (transfer_status < 0) {
        process_i2c_status(transfer_status);
    }

}

void print_temperature() {
    uint16_t temp_data = (read_data);
    uint16_t temp_data2 = ( ((*((transfer_sequence.buf[0]).data)) << 8) | *((transfer_sequence.buf[1]).data) );

    float temp_celcius = ((175.72 * temp_data) / 65536) - 46.85;
    float temp_celcius2 = ((175.72 * temp_data2) / 65536) - 46.85;

    uint16_t temp_celcius_int = (uint16_t) temp_celcius;
    uint16_t temp_celcius_int2 = (uint16_t) temp_celcius2;


    LOG_INFO("Temperature: %d -- %d C", temp_celcius_int, temp_celcius_int2);
}

/*
 * decodes the return value from a I2CSPM_Transfer() call
 * Logs relevant info to the console
 *
 * ret_value = returned value from I2CSPM_Transfer()
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
            LOG_ERROR("Unknown I2C return value");
            break;

    }
}

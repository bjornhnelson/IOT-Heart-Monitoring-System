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

#define MAX30101_ADDR 0xAA // 0x55 << 1

// enable logging for temperature results to be displayed in console
#define INCLUDE_LOG_DEBUG 1
#include "log.h"

I2C_TransferSeq_TypeDef transfer_sequence;

// heart sensor global variable for saving i2c read data
uint8_t heart_data[8];

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


// return pointer to global variable
uint8_t* get_heart_data() {
    return heart_data;
}

// clear buffer for debugging
void reset_heart_data() {
    for (int i=0; i<8; i++) {
        heart_data[i] = 0;
    }
}

// print buffer for debugging
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


// calls the API's i2c setup function with i2c settings in typedef
// handles load power management initialization
void init_i2c() {

    // call the library setup function
    I2CSPM_Init(&i2c_settings);

}

// load power management deinitialization
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

/*
 * Do an i2c write
 *
 * cmd = data to write
 * len = num bytes to write
 */
void i2c_write(uint8_t* cmd, int len) {

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

/*
 * Do an i2c read
 */
void i2c_read() {

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

/*
 * Do an i2c read with specified address and length
 *
 * save_addr = address to save data to
 * num_bytes = num bytes to read
 */
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

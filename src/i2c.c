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
static const uint8_t READ_CMD = 0xF3;

// how many tries for reading a temperature value before giving up
#define MAX_READ_ATTEMPTS 5

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

// data structure for storing i2c data and associated length
typedef struct {
        uint8_t  data[2]; // buffer for data
        uint16_t len; // number of bytes in buffer
} i2c_data;

// initialization of data structure
i2c_data buf;

// calls the API's i2c setup function with i2c settings in typedef
// handles load power management iniitalization
void init_i2c() {
    I2CSPM_Init(&i2c_settings);
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

/*
 * calls the API's i2c read function
 * master receives data from slave
 *
 * result = pointer to buffer for saving bytes
 * len = number of bytes to be read
 */

I2C_TransferReturn_TypeDef i2c_read(uint8_t* result, uint16_t len) {
    I2C_TransferSeq_TypeDef sequence;
    I2C_TransferReturn_TypeDef status;

    sequence.addr = SI7021_ADDR << 1;
    sequence.flags = I2C_FLAG_READ;
    sequence.buf[0].data = result;
    sequence.buf[0].len = len;

    status = I2CSPM_Transfer(I2C0, &sequence);
    return status;
}

/*
 * calls the API's i2c write function
 * master sends data to the slave
 *
 * command = pointer to buffer containing bytes that will be written
 * len = number of bytes to be written
 */
I2C_TransferReturn_TypeDef i2c_write(uint8_t* command, uint16_t len) {
    I2C_TransferSeq_TypeDef sequence;
    I2C_TransferReturn_TypeDef status;

    sequence.addr = SI7021_ADDR << 1;
    sequence.flags = I2C_FLAG_WRITE;
    sequence.buf[0].data = command;
    sequence.buf[0].len = len;

    status = I2CSPM_Transfer(I2C0, &sequence);
    return status;
}

// Performs a temperature reading on the si7021 sensor
void read_temp_from_si7021() {

    // initialization of i2c and sensor
    init_i2c();
    gpioSi7021Enable();

    // wait 80 ms for sensor to power up
    timerWaitUs(80000);

    I2C_TransferReturn_TypeDef status;
    buf.data[0] = READ_CMD;
    buf.len = 1;

    // perform the write operation
    status = i2c_write(buf.data, buf.len);

    // 10 ms delay before reading
    timerWaitUs(10000);

    uint8_t num_attempts = 1;
    buf.len = 2;

    // try reading 5 times
    while (num_attempts < MAX_READ_ATTEMPTS) {
        //LOG_INFO("Attempt # %d", num_attempts);

        // perform a read operation
        status = i2c_read(buf.data, buf.len);

        // read was successful
        if (status == i2cTransferDone) {
            uint16_t temp_data = (buf.data[0] << 8) | (buf.data[1]);
            float temp_celcius = ((175.72 * temp_data) / 65536) - 46.85;
            uint16_t temp_celcius_int = (uint16_t) temp_celcius;
            LOG_INFO("Temperature: %d C", temp_celcius_int);
            break;
        }
        // read returned an error
        else {
            process_i2c_status(status); // decode and log the error
            timerWaitUs(10000);
        }

        num_attempts++;
    }

    // deinitialization of i2c and sensor
    gpioSi7021Disable();
    deinit_i2c();

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
            LOG_WARN("I2CSPM_Transfer(): IN PROGRESS");
            break;
        case i2cTransferDone:
            LOG_INFO("I2CSPM_Transfer(): DONE");
            break;
        case i2cTransferNack:
            LOG_WARN("I2CSPM_Transfer(): NACK");
            break;
        case i2cTransferBusErr:
            LOG_WARN("I2CSPM_Transfer(): BUS ERR");
            break;
        case i2cTransferArbLost:
            LOG_WARN("I2CSPM_Transfer(): ARB LOST");
            break;
        case i2cTransferUsageFault:
            LOG_WARN("I2CSPM_Transfer(): USAGE FAULT");
            break;
        case i2cTransferSwFault:
            LOG_WARN("I2CSPM_Transfer(): SW FAULT");
            break;
        default:
            LOG_ERROR("Unknown I2CSPM_Transfer() return value");
            break;

    }
}

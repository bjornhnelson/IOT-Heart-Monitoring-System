/*
 * i2c.h
 *
 *  Created on: Feb 2, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_I2C_H_
#define SRC_I2C_H_

#include "stdint.h"
#include "em_i2c.h"

uint8_t* get_heart_data();

void print_heart_data();

void reset_heart_data();

int status_byte_error();

void i2c_write(uint8_t* cmd, int len);

void i2c_read();

void i2c_read_addr(uint8_t* save_addr, uint8_t num_bytes);

void init_i2c();

void deinit_i2c();

void i2c_send_command();

void i2c_receive_data();

uint16_t get_temp();

void process_i2c_status(I2C_TransferReturn_TypeDef ret_value);

#endif /* SRC_I2C_H_ */

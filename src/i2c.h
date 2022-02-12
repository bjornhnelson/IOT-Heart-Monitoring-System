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

void init_i2c();

void deinit_i2c();

void i2c_send_command();

void i2c_receive_data();

void print_temperature();

void process_i2c_status(I2C_TransferReturn_TypeDef ret_value);

#endif /* SRC_I2C_H_ */

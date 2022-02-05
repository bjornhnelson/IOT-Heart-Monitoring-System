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

I2C_TransferReturn_TypeDef i2c_read(uint8_t* result, uint16_t len);

I2C_TransferReturn_TypeDef i2c_write(uint8_t* command, uint16_t len);

void read_temp_from_si7021();

void process_i2c_status(I2C_TransferReturn_TypeDef ret_value);

#endif /* SRC_I2C_H_ */

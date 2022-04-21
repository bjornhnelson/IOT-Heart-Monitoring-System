/*
   gpio.h
  
    Created on: Dec 12, 2018
        Author: Dan Walkes

    Updated by Dave Sluiter Sept 7, 2020. moved #defines from .c to .h file.
    Updated by Dave Sluiter Dec 31, 2020. Minor edits with #defines.

 */

#ifndef SRC_GPIO_H_
#define SRC_GPIO_H_

#include "em_gpio.h"

#define PB0_PORT gpioPortF
#define PB0_PIN 6

#define PB1_PORT gpioPortF
#define PB1_PIN 7

#define MAX30101_PORT gpioPortD
#define MAX30101_MFIO_PIN 10
#define MAX30101_RESET_PIN 11

// Function prototypes
void init_GPIO();
void gpioLed0SetOn();
void gpioLed0SetOff();
void gpioLed1SetOn();
void gpioLed1SetOff();
void gpioI2cSclEnable();
void gpioI2cSclDisable();
void gpioI2cSdaEnable();
void gpioI2cSdaDisable();
void gpioSensorEnSetOn();
void gpioSensorEnSetOff();
void gpioSetDisplayExtcomin(int last_status_on);

#endif /* SRC_GPIO_H_ */

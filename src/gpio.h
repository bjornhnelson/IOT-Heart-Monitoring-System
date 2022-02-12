/*
   gpio.h
  
    Created on: Dec 12, 2018
        Author: Dan Walkes

    Updated by Dave Sluiter Sept 7, 2020. moved #defines from .c to .h file.
    Updated by Dave Sluiter Dec 31, 2020. Minor edits with #defines.

 */

#ifndef SRC_GPIO_H_
#define SRC_GPIO_H_

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
void gpioSi7021Enable();
void gpioSi7021Disable();

#endif /* SRC_GPIO_H_ */

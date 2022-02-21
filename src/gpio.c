/*
  gpio.c
 
   Created on: Dec 12, 2018
       Author: Dan Walkes
   Updated by Dave Sluiter Dec 31, 2020. Minor edits with #defines.

   March 17
   Dave Sluiter: Use this file to define functions that set up or control GPIOs.

 */


// *****************************************************************************
// Students:
// We will be creating additional functions that configure and manipulate GPIOs.
// For any new GPIO function you create, place that function in this file.
// *****************************************************************************

#include <stdbool.h>
#include "em_gpio.h"
#include <string.h>
#include "gpio.h"


// Student Edit: Define these, 0's are placeholder values.
// See the radio board user guide at https://www.silabs.com/documents/login/user-guides/ug279-brd4104a-user-guide.pdf
// and GPIO documentation at https://siliconlabs.github.io/Gecko_SDK_Doc/efm32g/html/group__GPIO.html
// to determine the correct values for these.

#define LED0_port  gpioPortF // change to correct ports and pins
#define LED0_pin   4
#define LED1_port  gpioPortF
#define LED1_pin   5


#define I2C_SCL_PORT gpioPortC
#define I2C_SCL_PIN 10
#define I2C_SDA_PORT gpioPortC
#define I2C_SDA_PIN 11

#define SI7021_PORT gpioPortD
#define SI7021_PIN 15

#define LCD_EXCOMIN_PORT gpioPortD
#define LCD_EXCOMIN_PIN 13


// Set GPIO drive strengths and modes of operation
void init_GPIO() {

	//GPIO_DriveStrengthSet(LED0_port, gpioDriveStrengthStrongAlternateStrong);
	GPIO_DriveStrengthSet(LED0_port, gpioDriveStrengthWeakAlternateWeak);
	GPIO_PinModeSet(LED0_port, LED0_pin, gpioModePushPull, false);

	//GPIO_DriveStrengthSet(LED1_port, gpioDriveStrengthStrongAlternateStrong);
	GPIO_DriveStrengthSet(LED1_port, gpioDriveStrengthWeakAlternateWeak);
	GPIO_PinModeSet(LED1_port, LED1_pin, gpioModePushPull, false);

}

// Turn on LED 0
void gpioLed0SetOn()
{
	GPIO_PinOutSet(LED0_port,LED0_pin);
}

// Turn off LED 0
void gpioLed0SetOff()
{
	GPIO_PinOutClear(LED0_port,LED0_pin);
}

// Turn on LED 1
void gpioLed1SetOn()
{
	GPIO_PinOutSet(LED1_port,LED1_pin);
}

// Turn off LED 1
void gpioLed1SetOff()
{
	GPIO_PinOutClear(LED1_port,LED1_pin);
}

// Turn on I2C SCL pin
void gpioI2cSclEnable() {
    GPIO_PinOutSet(I2C_SCL_PORT, I2C_SCL_PIN);
}

// Turn off I2C SCL pin
void gpioI2cSclDisable() {
    GPIO_PinOutClear(I2C_SCL_PORT, I2C_SCL_PIN);
}

// Turn on I2C SDA pin
void gpioI2cSdaEnable() {
    GPIO_PinOutSet(I2C_SDA_PORT, I2C_SDA_PIN);
}

// Turn off I2C SDA pin
void gpioI2cSdaDisable() {
    GPIO_PinOutClear(I2C_SDA_PORT, I2C_SDA_PIN);
}

// Turn on temperature sensor and LCD pin
void gpioSensorEnSetOn() {
    GPIO_PinOutSet(SI7021_PORT, SI7021_PIN);
}

// Turn off temperature sensor and LCD pin
void gpioSensorEnSetOff() {
    GPIO_PinOutClear(SI7021_PORT, SI7021_PIN);
}

// Set the EXTCOMIN input to the LCD
void gpioSetDisplayExtcomin(int last_status_on) {
    if (last_status_on) {
        GPIO_PinOutSet(LCD_EXCOMIN_PORT, LCD_EXCOMIN_PIN);
    }
    else {
        GPIO_PinOutClear(LCD_EXCOMIN_PORT, LCD_EXCOMIN_PIN);
    }
}


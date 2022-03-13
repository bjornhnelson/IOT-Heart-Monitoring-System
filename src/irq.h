/*
 * irq.h
 *
 *  Created on: Jan 24, 2022
 *      Author: bjornnelson
 */

#ifndef SRC_IRQ_H_
#define SRC_IRQ_H_

#include "stdint.h"

void I2C0_IRQHandler();
void LETIMER0_IRQHandler();
void GPIO_EVEN_IRQHandler();

void count_underflows();
uint32_t letimerMilliseconds();

#endif /* SRC_IRQ_H_ */

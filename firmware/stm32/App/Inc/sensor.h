/* sensor.h — TMP117 temperature sensor driver (STM32 HAL I2C)
 *
 * TMP117 I2C address: 0x48 (7-bit), ADD0=GND ADD1=GND
 * HAL DevAddress: 0x90 (0x48 << 1)
 * Shared I2C1 bus with ST25DV64K.
 */
#ifndef SENSOR_H
#define SENSOR_H

#include "stm32l0xx_hal.h"
#include <stdint.h>

/* Pass the HAL I2C handle from CubeMX (typically &hi2c1). */
void sensor_init(I2C_HandleTypeDef *hi2c);

/* Read temperature register (0x00). Returns raw 16-bit signed value.
 * Temperature in °C = raw × 0.0078125 */
int  sensor_read_temp(int16_t *raw_out);

#endif /* SENSOR_H */

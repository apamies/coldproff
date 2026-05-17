#ifndef __SENSOR_H__
#define __SENSOR_H__

#include <stdint.h>

int sensor_i2c_init(void);
int sensor_read_temp(int16_t *temp_raw);
int i2c_read_register(uint8_t reg, uint8_t *data, uint32_t len);
int i2c_write_register(uint8_t reg, uint8_t *data, uint32_t len);
void sensor_close(void);

#endif  // __SENSOR_H__

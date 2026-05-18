/*
 * sensor.c — TMP117 temperature sensor (STM32 HAL I2C)
 *
 * TMP117 I2C: 7-bit addr 0x48 (ADD0=GND, ADD1=GND).
 * HAL DevAddress = 0x48 << 1 = 0x90.
 * Register 0x00 returns 16-bit 2's-complement temperature.
 * Resolution: 7.8125 m°C per LSB.
 */

#include "sensor.h"
#include "coldproff_log.h"

#define TAG "SENSOR"

#define TMP117_HAL_ADDR  0x90   /* 0x48 << 1 */
#define TMP117_REG_TEMP  0x00
#define TMP117_REG_CFG   0x01
#define TMP117_REG_ID    0x0F   /* expected 0x0117 */

static I2C_HandleTypeDef *hi2c = NULL;

/* ------------------------------------------------------------------ */
void sensor_init(I2C_HandleTypeDef *h) {
    hi2c = h;

    /* Set continuous conversion, 1 Hz (config word 0x0200) */
    uint8_t cfg[2] = {0x02, 0x00};
    if (HAL_I2C_Mem_Write(hi2c, TMP117_HAL_ADDR, TMP117_REG_CFG,
                          I2C_MEMADD_SIZE_8BIT, cfg, 2, 100) != HAL_OK) {
        LOG_E(TAG, "TMP117 config write failed");
    }
}

/* ------------------------------------------------------------------ */
int sensor_read_temp(int16_t *raw_out) {
    uint8_t data[2];
    if (HAL_I2C_Mem_Read(hi2c, TMP117_HAL_ADDR, TMP117_REG_TEMP,
                         I2C_MEMADD_SIZE_8BIT, data, 2, 100) != HAL_OK) {
        LOG_E(TAG, "TMP117 I2C read failed");
        return -1;
    }
    *raw_out = (int16_t)((data[0] << 8) | data[1]);
    LOG_D(TAG, "TMP117 raw=0x%04X (%.2f °C)",
          (unsigned)*raw_out, (float)*raw_out * 0.0078125f);
    return 0;
}

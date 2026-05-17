/*
 * TMP117 Temperature Sensor interface
 * I2C: 0xB0 (write) / 0xB1 (read) - configurable via address pins
 * Typical: I2C1 on BC660K-GL
 */

#include "ql_api_common.h"
#include "ql_log.h"
#include "ql_i2c.h"
#include "sensor.h"
#include <string.h>

#define TAG "SENSOR"

// TMP117 I2C address (A0=GND, A1=GND → 0x48 7-bit)
#define TMP117_I2C_ADDR 0x48

// TMP117 registers
#define TMP117_REG_TEMP_RESULT   0x00  // Read-only, Temperature result
#define TMP117_REG_CONFIG        0x01  // Configuration
#define TMP117_REG_HIGH_LIMIT    0x02
#define TMP117_REG_LOW_LIMIT     0x03
#define TMP117_REG_DEVICE_ID     0x0F  // 0x0117

static ql_i2c_t i2c_dev = NULL;

int sensor_i2c_init(void) {
    QL_LOG_INFO(TAG, "Initializing I2C for TMP117...");

    // Open I2C bus (typically I2C0 or I2C1 on BC660K-GL)
    // Standard mode 100kHz is sufficient for TMP117
    i2c_dev = ql_i2c_open(1, 100);  // I2C1, 100kHz
    if (!i2c_dev) {
        QL_LOG_ERR(TAG, "Failed to open I2C1");
        return -1;
    }

    // Verify device ID
    uint8_t device_id[2];
    if (i2c_read_register(TMP117_REG_DEVICE_ID, device_id, 2) != 0) {
        QL_LOG_ERR(TAG, "Failed to read device ID");
        return -1;
    }

    uint16_t id = (device_id[0] << 8) | device_id[1];
    QL_LOG_INFO(TAG, "TMP117 Device ID: 0x%04X", id);

    if (id != 0x0117) {
        QL_LOG_ERR(TAG, "Invalid device ID (expected 0x0117)");
        return -1;
    }

    // Configure: continuous conversion, 1 Hz sampling
    // Config register: [AVG(2) | Mode(2) | CR(2) | POL(1) | DR(1) | RST(1)]
    // Mode 0 = continuous
    // CR = 00 (250ms conversion time @ 1Hz)
    uint8_t config[2] = {0x02, 0x00};  // Continuous mode, 1 Hz
    if (i2c_write_register(TMP117_REG_CONFIG, config, 2) != 0) {
        QL_LOG_ERR(TAG, "Failed to configure TMP117");
        return -1;
    }

    QL_LOG_INFO(TAG, "TMP117 configured");
    return 0;
}

int i2c_read_register(uint8_t reg, uint8_t *data, uint32_t len) {
    if (!i2c_dev) {
        QL_LOG_ERR(TAG, "I2C not initialized");
        return -1;
    }

    // Write register address
    uint8_t cmd[1] = {reg};
    int ret = ql_i2c_write(i2c_dev, TMP117_I2C_ADDR, cmd, 1);
    if (ret != 0) {
        QL_LOG_ERR(TAG, "I2C write (reg addr) failed");
        return -1;
    }

    // Small delay for I2C setup
    ql_rtos_task_sleep_ms(1);

    // Read data
    ret = ql_i2c_read(i2c_dev, TMP117_I2C_ADDR, data, len);
    if (ret != 0) {
        QL_LOG_ERR(TAG, "I2C read failed");
        return -1;
    }

    return 0;
}

int i2c_write_register(uint8_t reg, uint8_t *data, uint32_t len) {
    if (!i2c_dev) {
        QL_LOG_ERR(TAG, "I2C not initialized");
        return -1;
    }

    uint8_t buf[len + 1];
    buf[0] = reg;
    memcpy(buf + 1, data, len);

    int ret = ql_i2c_write(i2c_dev, TMP117_I2C_ADDR, buf, len + 1);
    if (ret != 0) {
        QL_LOG_ERR(TAG, "I2C write failed");
        return -1;
    }

    return 0;
}

int sensor_read_temp(int16_t *temp_raw) {
    uint8_t temp_bytes[2];

    if (i2c_read_register(TMP117_REG_TEMP_RESULT, temp_bytes, 2) != 0) {
        QL_LOG_ERR(TAG, "Failed to read temperature");
        return -1;
    }

    // TMP117 returns 16-bit signed temperature
    // MSB first: [T15:T8] [T7:T0]
    // Resolution: 0.0078125°C per LSB
    *temp_raw = (int16_t)((temp_bytes[0] << 8) | temp_bytes[1]);

    // Convert to millidegrees Celsius for storage (avoid floating point)
    // temp_mC = temp_raw * 7.8125 / 1000 * 1000 = temp_raw * 7.8125
    int32_t temp_mc = (*temp_raw * 7812) / 1000;  // Fixed-point math

    QL_LOG_DEBUG(TAG, "Temp raw: 0x%04X → %d mC", *temp_raw, temp_mc);

    return 0;
}

void sensor_close(void) {
    if (i2c_dev) {
        ql_i2c_close(i2c_dev);
        i2c_dev = NULL;
    }
}

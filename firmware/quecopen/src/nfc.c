/*
 * ST25DV64K NFC + EEPROM interface
 * Dual-port: I2C (MCU) + NFC (phone)
 * I2C addresses: 0xA0 (E2) EEPROM, 0xAE (RF) RF interface
 * 8KB total capacity
 */

#include "ql_api_common.h"
#include "ql_log.h"
#include "ql_i2c.h"
#include "nfc.h"
#include <string.h>
#include <stdio.h>

#define TAG "NFC"

// ST25DV64K I2C addresses (7-bit)
#define ST25DV_EEPROM_ADDR  0x50  // E2 (0xA0 with R/W bit)
#define ST25DV_RF_ADDR      0x57  // RF (0xAE with R/W bit)

// EEPROM memory layout
#define ST25DV_SYSTEM_AREA  0x00   // System configuration
#define ST25DV_USER_AREA    0x04   // User memory starts at byte 4
#define ST25DV_TOTAL_SIZE   8192   // 8KB

// Record format in ST25DV EEPROM:
// Byte 0-3: Timestamp (seconds since epoch, little-endian)
// Byte 4-5: Temperature (int16_t raw)
// Byte 6-7: MCC
// Byte 8-9: MNC
// Byte 10-11: TAC (hex)
// Byte 12-15: Cell ID (hex)
// Byte 16: RSRP
// Byte 17: Reserved/flags
// Total: 18 bytes per record

#define RECORD_SIZE 18
#define MAX_RECORDS (ST25DV_TOTAL_SIZE - ST25DV_USER_AREA) / RECORD_SIZE

static ql_i2c_t i2c_dev_nfc = NULL;
static uint32_t record_offset = ST25DV_USER_AREA;

int nfc_i2c_init(void) {
    QL_LOG_INFO(TAG, "Initializing I2C for ST25DV64K...");

    // Use same I2C bus as sensor (I2C1)
    i2c_dev_nfc = ql_i2c_open(1, 100);
    if (!i2c_dev_nfc) {
        QL_LOG_ERR(TAG, "Failed to open I2C1");
        return -1;
    }

    // Read system configuration to verify device presence
    uint8_t gpo_reg[1];
    if (nfc_i2c_read(ST25DV_SYSTEM_AREA, gpo_reg, 1) != 0) {
        QL_LOG_ERR(TAG, "Failed to detect ST25DV64K");
        return -1;
    }

    QL_LOG_INFO(TAG, "ST25DV64K detected (system reg: 0x%02X)", gpo_reg[0]);

    return 0;
}

static int nfc_i2c_read(uint16_t addr, uint8_t *data, uint32_t len) {
    if (!i2c_dev_nfc) {
        QL_LOG_ERR(TAG, "I2C not initialized");
        return -1;
    }

    // ST25DV uses 2-byte addressing
    uint8_t addr_bytes[2] = {(uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};

    // Write address
    if (ql_i2c_write(i2c_dev_nfc, ST25DV_EEPROM_ADDR, addr_bytes, 2) != 0) {
        QL_LOG_ERR(TAG, "I2C address write failed");
        return -1;
    }

    ql_rtos_task_sleep_ms(1);

    // Read data
    if (ql_i2c_read(i2c_dev_nfc, ST25DV_EEPROM_ADDR, data, len) != 0) {
        QL_LOG_ERR(TAG, "I2C read failed");
        return -1;
    }

    return 0;
}

static int nfc_i2c_write(uint16_t addr, uint8_t *data, uint32_t len) {
    if (!i2c_dev_nfc) {
        QL_LOG_ERR(TAG, "I2C not initialized");
        return -1;
    }

    // Combined write: address (2 bytes) + data
    uint8_t buf[len + 2];
    buf[0] = (uint8_t)(addr >> 8);
    buf[1] = (uint8_t)(addr & 0xFF);
    memcpy(buf + 2, data, len);

    if (ql_i2c_write(i2c_dev_nfc, ST25DV_EEPROM_ADDR, buf, len + 2) != 0) {
        QL_LOG_ERR(TAG, "I2C write failed");
        return -1;
    }

    // ST25DV write cycle time ~5ms
    ql_rtos_task_sleep_ms(5);

    return 0;
}

int nfc_write_record(CellData_t *cell_data) {
    if (record_offset + RECORD_SIZE > ST25DV_TOTAL_SIZE) {
        QL_LOG_ERR(TAG, "EEPROM full (offset: %lu)", record_offset);
        return -1;
    }

    uint8_t record[RECORD_SIZE];
    uint32_t i = 0;

    // Timestamp (placeholder: use system time in production)
    uint32_t timestamp = 0x12345678;  // TODO: Get real timestamp
    record[i++] = (uint8_t)(timestamp & 0xFF);
    record[i++] = (uint8_t)((timestamp >> 8) & 0xFF);
    record[i++] = (uint8_t)((timestamp >> 16) & 0xFF);
    record[i++] = (uint8_t)((timestamp >> 24) & 0xFF);

    // Temperature (int16_t)
    record[i++] = (uint8_t)(cell_data->temp_raw & 0xFF);
    record[i++] = (uint8_t)((cell_data->temp_raw >> 8) & 0xFF);

    // MCC (uint16_t)
    record[i++] = (uint8_t)(cell_data->mcc & 0xFF);
    record[i++] = (uint8_t)((cell_data->mcc >> 8) & 0xFF);

    // MNC (uint16_t)
    record[i++] = (uint8_t)(cell_data->mnc & 0xFF);
    record[i++] = (uint8_t)((cell_data->mnc >> 8) & 0xFF);

    // TAC (uint16_t)
    record[i++] = (uint8_t)(cell_data->tac & 0xFF);
    record[i++] = (uint8_t)((cell_data->tac >> 8) & 0xFF);

    // Cell ID (uint32_t)
    record[i++] = (uint8_t)(cell_data->cell_id & 0xFF);
    record[i++] = (uint8_t)((cell_data->cell_id >> 8) & 0xFF);
    record[i++] = (uint8_t)((cell_data->cell_id >> 16) & 0xFF);
    record[i++] = (uint8_t)((cell_data->cell_id >> 24) & 0xFF);

    // RSRP (int16_t)
    record[i++] = (uint8_t)(cell_data->rsrp & 0xFF);
    record[i++] = (uint8_t)((cell_data->rsrp >> 8) & 0xFF);

    // Flags/reserved
    record[i++] = 0x00;

    // Write to EEPROM
    if (nfc_i2c_write(record_offset, record, RECORD_SIZE) != 0) {
        QL_LOG_ERR(TAG, "Failed to write record at offset %lu", record_offset);
        return -1;
    }

    QL_LOG_DEBUG(TAG, "Record written at offset %lu (CID=0x%X TAC=0x%X Temp=%d)",
                record_offset, cell_data->cell_id, cell_data->tac, cell_data->temp_raw);

    record_offset += RECORD_SIZE;
    return 0;
}

int nfc_get_geolocation_url(char *url_buf, uint32_url_size) {
    // Build URL for NFC readout
    // Example: https://example.com/api/cell?cid=110324069&tac=2250&mcc=214&mnc=3
    // This will be encoded as NDEF T4T record

    snprintf(url_buf, url_size, "https://example.com/api/geoloc");

    return 0;
}

uint32_t nfc_get_record_count(void) {
    uint32_t used_size = record_offset - ST25DV_USER_AREA;
    return used_size / RECORD_SIZE;
}

void nfc_reset_storage(void) {
    record_offset = ST25DV_USER_AREA;
    // Optional: erase all bytes
}

void nfc_close(void) {
    if (i2c_dev_nfc) {
        ql_i2c_close(i2c_dev_nfc);
        i2c_dev_nfc = NULL;
    }
}

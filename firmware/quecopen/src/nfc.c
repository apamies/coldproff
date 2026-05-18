/*
 * ST25DV64K NFC + EEPROM interface
 * Dual-port: I2C (MCU side) + NFC RF (phone side)
 * I2C: 7-bit 0x50 (EEPROM), 0x57 (system registers)
 * Capacity: 8KB (0x0000–0x1FFF)
 *
 * Memory layout:
 *   0x0000–0x00FF : NDEF TLV area (current geolocation URL, 256 bytes)
 *   0x0100–0x1FFF : Sensor records, 18 bytes each, up to 440 records
 */

#include "ql_api_common.h"
#include "ql_log.h"
#include "ql_i2c.h"
#include "nfc.h"
#include "ndef.h"
#include "geoloc.h"
#include <string.h>
#include <stdio.h>

#define TAG "NFC"

#define ST25DV_EEPROM_ADDR  0x50   // 7-bit (0xA0 with R/W bit)
#define ST25DV_SYS_ADDR     0x57   // 7-bit system registers (0xAE with R/W bit)

// Memory map
#define NDEF_AREA_START     0x0000
#define NDEF_AREA_SIZE      0x0100   // 256 bytes for NDEF
#define RECORDS_START       0x0100   // Sensor records start after NDEF area

// Each sensor record: 18 bytes
// Byte  0-3 : Timestamp (uint32_t LE)
// Byte  4-5 : Temperature raw (int16_t LE)
// Byte  6-7 : MCC (uint16_t LE)
// Byte  8-9 : MNC (uint16_t LE)
// Byte 10-11: TAC (uint16_t LE)
// Byte 12-15: Cell ID (uint32_t LE)
// Byte 16-17: RSRP (int16_t LE)
#define RECORD_SIZE  18
#define MAX_RECORDS  ((0x2000 - RECORDS_START) / RECORD_SIZE)   // 454 records

static ql_i2c_t i2c_dev_nfc    = NULL;
static uint16_t record_offset  = RECORDS_START;
static CellData_t last_cell    = {0};   // Cache for NFC URL generation

// Forward declarations for static helpers
static int st25dv_i2c_read(uint16_t addr, uint8_t *data, uint32_t len);
static int st25dv_i2c_write(uint16_t addr, uint8_t *data, uint32_t len);

int nfc_i2c_init(void) {
    QL_LOG_INFO(TAG, "Initializing I2C for ST25DV64K...");

    i2c_dev_nfc = ql_i2c_open(1, 100);  // I2C bus 1, 100 kHz
    if (!i2c_dev_nfc) {
        QL_LOG_ERR(TAG, "Failed to open I2C1");
        return -1;
    }

    // Read first byte of EEPROM to verify device presence
    uint8_t probe;
    if (st25dv_i2c_read(0x0000, &probe, 1) != 0) {
        QL_LOG_ERR(TAG, "ST25DV64K not responding on I2C");
        return -1;
    }

    QL_LOG_INFO(TAG, "ST25DV64K detected (byte[0]=0x%02X)", probe);
    return 0;
}

static int st25dv_i2c_read(uint16_t addr, uint8_t *data, uint32_t len) {
    if (!i2c_dev_nfc) {
        QL_LOG_ERR(TAG, "I2C not initialized");
        return -1;
    }

    uint8_t addr_bytes[2] = {(uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};

    if (ql_i2c_write(i2c_dev_nfc, ST25DV_EEPROM_ADDR, addr_bytes, 2) != 0) {
        QL_LOG_ERR(TAG, "I2C address phase failed");
        return -1;
    }

    ql_rtos_task_sleep_ms(1);

    if (ql_i2c_read(i2c_dev_nfc, ST25DV_EEPROM_ADDR, data, len) != 0) {
        QL_LOG_ERR(TAG, "I2C read failed at 0x%04X", addr);
        return -1;
    }

    return 0;
}

static int st25dv_i2c_write(uint16_t addr, uint8_t *data, uint32_t len) {
    if (!i2c_dev_nfc) {
        QL_LOG_ERR(TAG, "I2C not initialized");
        return -1;
    }

    // Combined buffer: 2-byte address + payload
    // Max chunk is 64 bytes so total is 66 bytes maximum
    uint8_t buf[66];
    if (len > 64) {
        QL_LOG_ERR(TAG, "Write chunk too large (%u > 64)", len);
        return -1;
    }

    buf[0] = (uint8_t)(addr >> 8);
    buf[1] = (uint8_t)(addr & 0xFF);
    memcpy(buf + 2, data, len);

    if (ql_i2c_write(i2c_dev_nfc, ST25DV_EEPROM_ADDR, buf, len + 2) != 0) {
        QL_LOG_ERR(TAG, "I2C write failed at 0x%04X", addr);
        return -1;
    }

    // ST25DV64K EEPROM write cycle time (datasheet: 5 ms typical)
    ql_rtos_task_sleep_ms(5);
    return 0;
}

/*
 * Serialize CellData_t into 18-byte EEPROM record and write it.
 * Also rebuilds the NDEF TLV block at 0x0000 with the new cell data URL.
 */
int nfc_write_record(CellData_t *cell_data) {
    if (!cell_data) {
        QL_LOG_ERR(TAG, "NULL cell_data");
        return -1;
    }

    if (record_offset + RECORD_SIZE > 0x2000) {
        QL_LOG_ERR(TAG, "EEPROM sensor area full (offset 0x%04X)", record_offset);
        return -1;
    }

    // Serialize record
    uint8_t record[RECORD_SIZE];
    uint32_t i = 0;

    // Bytes 0-3: Unix UTC timestamp from modem_get_unix_time() (0 = unavailable)
    uint32_t timestamp = cell_data->timestamp;
    record[i++] = (uint8_t)(timestamp & 0xFF);
    record[i++] = (uint8_t)((timestamp >>  8) & 0xFF);
    record[i++] = (uint8_t)((timestamp >> 16) & 0xFF);
    record[i++] = (uint8_t)((timestamp >> 24) & 0xFF);

    // Bytes 4-5: temperature (TMP117 raw, 7.8125 m°C per LSB)
    record[i++] = (uint8_t)(cell_data->temp_raw & 0xFF);
    record[i++] = (uint8_t)((cell_data->temp_raw >> 8) & 0xFF);

    // Bytes 6-7: MCC
    record[i++] = (uint8_t)(cell_data->mcc & 0xFF);
    record[i++] = (uint8_t)((cell_data->mcc >> 8) & 0xFF);

    // Bytes 8-9: MNC
    record[i++] = (uint8_t)(cell_data->mnc & 0xFF);
    record[i++] = (uint8_t)((cell_data->mnc >> 8) & 0xFF);

    // Bytes 10-11: TAC (already decimal after hex parse in modem.c)
    record[i++] = (uint8_t)(cell_data->tac & 0xFF);
    record[i++] = (uint8_t)((cell_data->tac >> 8) & 0xFF);

    // Bytes 12-15: Cell ID
    record[i++] = (uint8_t)(cell_data->cell_id & 0xFF);
    record[i++] = (uint8_t)((cell_data->cell_id >>  8) & 0xFF);
    record[i++] = (uint8_t)((cell_data->cell_id >> 16) & 0xFF);
    record[i++] = (uint8_t)((cell_data->cell_id >> 24) & 0xFF);

    // Bytes 16-17: RSRP
    record[i++] = (uint8_t)(cell_data->rsrp & 0xFF);
    record[i++] = (uint8_t)((cell_data->rsrp >> 8) & 0xFF);

    if (st25dv_i2c_write(record_offset, record, RECORD_SIZE) != 0) {
        QL_LOG_ERR(TAG, "Failed to write record at 0x%04X", record_offset);
        return -1;
    }

    QL_LOG_DEBUG(TAG, "Record[%u] at 0x%04X: CID=0x%X TAC=0x%X Temp=%d RSRP=%d",
                 nfc_get_record_count(), record_offset,
                 cell_data->cell_id, cell_data->tac,
                 cell_data->temp_raw, (int)cell_data->rsrp);

    record_offset += RECORD_SIZE;

    // Cache latest cell data for URL generation
    last_cell = *cell_data;

    // Update NDEF TLV block so NFC read shows current location link
    uint8_t ndef_msg[256];
    int ndef_len = ndef_build_sensor_record(cell_data, ndef_msg, sizeof(ndef_msg));
    if (ndef_len < 0) {
        QL_LOG_ERR(TAG, "Failed to build NDEF sensor record");
        return -1;
    }

    if (ndef_write_to_eeprom(ndef_msg, (uint32_t)ndef_len, st25dv_i2c_write) != 0) {
        QL_LOG_ERR(TAG, "Failed to write NDEF to EEPROM");
        return -1;
    }

    return 0;
}

/*
 * Build geolocation URL from the last stored cell data.
 * Returns URL length, -1 on error.
 */
int nfc_get_geolocation_url(char *url_buf, uint32_t url_size) {
    if (!url_buf || url_size < 64) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    if (last_cell.cell_id == 0) {
        QL_LOG_ERR(TAG, "No cell data available yet");
        return -1;
    }

    return geoloc_encode_url_params(&last_cell, url_buf, url_size);
}

uint32_t nfc_get_record_count(void) {
    return (record_offset - RECORDS_START) / RECORD_SIZE;
}

void nfc_reset_storage(void) {
    record_offset = RECORDS_START;
    memset(&last_cell, 0, sizeof(last_cell));
}

void nfc_close(void) {
    if (i2c_dev_nfc) {
        ql_i2c_close(i2c_dev_nfc);
        i2c_dev_nfc = NULL;
    }
}

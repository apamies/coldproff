/*
 * nfc.c — ST25DV64K NFC + EEPROM driver (STM32 HAL I2C)
 *
 * EEPROM I2C: 7-bit 0x50 → HAL DevAddress 0xA0
 * SYS    I2C: 7-bit 0x57 → HAL DevAddress 0xAE
 *
 * Memory layout (8 KB):
 *   0x0000–0x00FF : NDEF TLV (URL, updated each cycle)
 *   0x0100–0x1FFF : Sensor records, 18 bytes each
 *
 * Record layout (18 bytes, all little-endian):
 *   Byte  0- 3 : Timestamp Unix UTC (uint32_t)
 *   Byte  4- 5 : TMP117 raw (int16_t)
 *   Byte  6- 7 : MCC (uint16_t)
 *   Byte  8- 9 : MNC (uint16_t)
 *   Byte 10-11 : TAC (uint16_t, hex-parsed)
 *   Byte 12-15 : Cell ID (uint32_t, hex-parsed)
 *   Byte 16-17 : RSRP (int16_t)
 */

#include "nfc.h"
#include "ndef.h"
#include "geoloc.h"
#include "coldproff_log.h"
#include <string.h>

#define TAG "NFC"

#define ST25DV_EEPROM_HAL_ADDR  0xA0
#define ST25DV_SYS_HAL_ADDR     0xAE

#define NDEF_AREA_START  0x0000
#define RECORDS_START    0x0100
#define RECORD_SIZE      18
#define MAX_RECORDS      ((0x2000 - RECORDS_START) / RECORD_SIZE)  /* 454 */

static I2C_HandleTypeDef *hi2c       = NULL;
static uint16_t           rec_offset = RECORDS_START;
static CellData_t         last_cell  = {0};

/* ------------------------------------------------------------------ */
/* Low-level I2C read/write with 16-bit EEPROM address                */
/* ------------------------------------------------------------------ */
static int st25dv_write(uint16_t addr, uint8_t *data, uint32_t len) {
    if (len > 64) return -1;    /* ST25DV page size = 64 bytes */

    uint8_t buf[66];
    buf[0] = (uint8_t)(addr >> 8);
    buf[1] = (uint8_t)(addr & 0xFF);
    memcpy(buf + 2, data, len);

    if (HAL_I2C_Master_Transmit(hi2c, ST25DV_EEPROM_HAL_ADDR,
                                 buf, (uint16_t)(len + 2), 100) != HAL_OK) {
        LOG_E(TAG, "I2C write @ 0x%04X failed", addr);
        return -1;
    }
    HAL_Delay(5);   /* EEPROM write cycle: 5 ms typ (ST25DV64K datasheet) */
    return 0;
}

static int st25dv_read(uint16_t addr, uint8_t *data, uint32_t len) {
    uint8_t a[2] = {(uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};
    if (HAL_I2C_Master_Transmit(hi2c, ST25DV_EEPROM_HAL_ADDR,
                                 a, 2, 100) != HAL_OK) return -1;
    HAL_Delay(1);
    if (HAL_I2C_Master_Receive(hi2c, ST25DV_EEPROM_HAL_ADDR,
                                data, (uint16_t)len, 100) != HAL_OK) return -1;
    return 0;
}

/* Adapter so ndef_write_to_eeprom() can use our write function */
static int ndef_i2c_write(uint16_t addr, uint8_t *data, uint32_t len) {
    return st25dv_write(addr, data, len);
}

/* ------------------------------------------------------------------ */
int nfc_init(I2C_HandleTypeDef *h) {
    hi2c = h;

    /* Probe device */
    uint8_t probe;
    if (st25dv_read(0x0000, &probe, 1) != 0) {
        LOG_E(TAG, "ST25DV64K not found on I2C");
        return -1;
    }
    LOG_I(TAG, "ST25DV64K detected (byte[0]=0x%02X)", probe);
    return 0;
}

/* ------------------------------------------------------------------ */
int nfc_write_record(CellData_t *cell_data) {
    if (!cell_data) return -1;
    if (rec_offset + RECORD_SIZE > 0x2000) {
        LOG_E(TAG, "EEPROM full at 0x%04X", rec_offset);
        return -1;
    }

    /* Serialize 18-byte record (little-endian) */
    uint8_t rec[RECORD_SIZE];
    uint32_t i = 0;

    uint32_t ts = cell_data->timestamp;
    rec[i++] = ts        & 0xFF;
    rec[i++] = (ts >>  8) & 0xFF;
    rec[i++] = (ts >> 16) & 0xFF;
    rec[i++] = (ts >> 24) & 0xFF;

    rec[i++] = cell_data->temp_raw & 0xFF;
    rec[i++] = (cell_data->temp_raw >> 8) & 0xFF;

    rec[i++] = cell_data->mcc & 0xFF;
    rec[i++] = (cell_data->mcc >> 8) & 0xFF;

    rec[i++] = cell_data->mnc & 0xFF;
    rec[i++] = (cell_data->mnc >> 8) & 0xFF;

    rec[i++] = cell_data->tac & 0xFF;
    rec[i++] = (cell_data->tac >> 8) & 0xFF;

    uint32_t cid = cell_data->cell_id;
    rec[i++] = cid        & 0xFF;
    rec[i++] = (cid >>  8) & 0xFF;
    rec[i++] = (cid >> 16) & 0xFF;
    rec[i++] = (cid >> 24) & 0xFF;

    rec[i++] = cell_data->rsrp & 0xFF;
    rec[i++] = (cell_data->rsrp >> 8) & 0xFF;

    if (st25dv_write(rec_offset, rec, RECORD_SIZE) != 0) return -1;

    LOG_I(TAG, "Record[%u] @ 0x%04X ts=%lu CID=0x%lX T=%d RSRP=%d",
          nfc_get_record_count(), rec_offset,
          (unsigned long)cell_data->timestamp,
          (unsigned long)cell_data->cell_id,
          cell_data->temp_raw, (int)cell_data->rsrp);

    rec_offset += RECORD_SIZE;
    last_cell = *cell_data;

    /* Rebuild NDEF TLV with latest cell data URL */
    uint8_t ndef_msg[240];
    int ndef_len = ndef_build_sensor_record(cell_data, ndef_msg, sizeof(ndef_msg));
    if (ndef_len < 0) return -1;
    return ndef_write_to_eeprom(ndef_msg, (uint32_t)ndef_len, ndef_i2c_write);
}

/* ------------------------------------------------------------------ */
uint32_t nfc_get_record_count(void) {
    return (rec_offset - RECORDS_START) / RECORD_SIZE;
}

void nfc_reset_storage(void) {
    rec_offset = RECORDS_START;
    memset(&last_cell, 0, sizeof(last_cell));
}

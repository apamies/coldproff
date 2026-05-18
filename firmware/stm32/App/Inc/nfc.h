/* nfc.h — ST25DV64K NFC + EEPROM driver (STM32 HAL I2C)
 *
 * I2C EEPROM addr: 0x50 (7-bit) → HAL DevAddress 0xA0
 * I2C SYS addr:   0x57 (7-bit) → HAL DevAddress 0xAE
 * Shared I2C1 bus with TMP117.
 *
 * Memory layout (8 KB):
 *   0x0000–0x00FF : NDEF TLV area (URL record, updated each cycle)
 *   0x0100–0x1FFF : Sensor records (18 bytes × up to 454 readings)
 */
#ifndef NFC_H
#define NFC_H

#include "stm32l0xx_hal.h"
#include "modem.h"
#include <stdint.h>

/* Pass the HAL I2C handle from CubeMX. */
int  nfc_init(I2C_HandleTypeDef *hi2c);

/* Serialize CellData_t into an 18-byte EEPROM record and write it.
 * Also updates the NDEF TLV block with the latest cell info URL.
 * Returns 0 on success. */
int  nfc_write_record(CellData_t *cell_data);

/* Number of records stored since last nfc_reset_storage(). */
uint32_t nfc_get_record_count(void);

/* Reset write pointer (does not erase EEPROM content). */
void nfc_reset_storage(void);

#endif /* NFC_H */

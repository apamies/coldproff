#ifndef __NDEF_H__
#define __NDEF_H__

#include <stdint.h>
#include "modem.h"

// NDEF TLV block types (ISO 15693 / NFC Forum memory layout)
#define NDEF_TLV_TYPE        0x03   // NDEF Message TLV
#define NDEF_TLV_TERMINATOR  0xFE   // Terminator TLV

// Build NDEF URI record (Well-Known, type 'U', full URL)
// Returns bytes written, or -1 on error
int ndef_build_url_record(const char *url, uint8_t *ndef_buf, uint32_t buf_size);

// Build NDEF Text record (Well-Known, type 'T', language "en")
// Returns bytes written, or -1 on error
int ndef_build_text_record(const char *text, uint8_t *ndef_buf, uint32_t buf_size);

// Build combined NDEF message: URL record (MB) + Text record (ME)
// Returns total bytes, or -1 on error
int ndef_build_sensor_record(CellData_t *cell_data, uint8_t *ndef_buf, uint32_t buf_size);

// Write NDEF to EEPROM in TLV format: [0x03][len][NDEF][0xFE]
// i2c_write_fn: platform I2C write at given 16-bit EEPROM address
int ndef_write_to_eeprom(const uint8_t *ndef_data, uint32_t ndef_len,
                         int (*i2c_write_fn)(uint16_t addr, uint8_t *data, uint32_t len));

// Parse NDEF URL from TLV-wrapped EEPROM buffer
// Returns URL length, or -1 on error
int ndef_parse_message(const uint8_t *ndef_buf, uint32_t buf_len,
                       char *url_out, uint32_t url_size);

#endif  // __NDEF_H__

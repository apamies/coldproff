#ifndef NDEF_H
#define NDEF_H

#include <stdint.h>
#include "modem.h"

#define NDEF_TLV_TYPE        0x03
#define NDEF_TLV_TERMINATOR  0xFE

int ndef_build_url_record(const char *url, uint8_t *ndef_buf, uint32_t buf_size);
int ndef_build_text_record(const char *text, uint8_t *ndef_buf, uint32_t buf_size);
int ndef_build_sensor_record(CellData_t *cell_data, uint8_t *ndef_buf, uint32_t buf_size);
int ndef_write_to_eeprom(const uint8_t *ndef_data, uint32_t ndef_len,
                         int (*i2c_write_fn)(uint16_t addr, uint8_t *data, uint32_t len));
int ndef_parse_message(const uint8_t *ndef_buf, uint32_t buf_len,
                       char *url_out, uint32_t url_size);

#endif /* NDEF_H */

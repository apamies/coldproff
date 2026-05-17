#ifndef __NFC_H__
#define __NFC_H__

#include <stdint.h>
#include "modem.h"

int nfc_i2c_init(void);
int nfc_write_record(CellData_t *cell_data);
int nfc_get_geolocation_url(char *url_buf, uint32_t url_size);
uint32_t nfc_get_record_count(void);
void nfc_reset_storage(void);
void nfc_close(void);

#endif  // __NFC_H__

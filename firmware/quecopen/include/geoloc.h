#ifndef __GEOLOC_H__
#define __GEOLOC_H__

#include <stdint.h>
#include "modem.h"

// Geolocation API structures
typedef struct {
    uint32_t mcc;
    uint32_t mnc;
    uint32_t cell_id;
    uint16_t lac;
} CellTowerInfo_t;

// Build geolocation query for Google API
int geoloc_build_google_query(CellData_t *cell_data, char *json_buf, uint32_t json_size);

// Build geolocation query for HERE API
int geoloc_build_here_query(CellData_t *cell_data, char *json_buf, uint32_t json_size);

// URL encoding for NFC payload
int geoloc_encode_url_params(CellData_t *cell_data, char *url_buf, uint32_t url_size);

#endif  // __GEOLOC_H__

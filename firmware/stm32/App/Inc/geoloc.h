#ifndef GEOLOC_H
#define GEOLOC_H

#include <stdint.h>
#include "modem.h"

typedef enum {
    GEOLOC_EXCELLENT = 0,
    GEOLOC_GOOD      = 1,
    GEOLOC_ACCEPTABLE = 2,
    GEOLOC_POOR      = 3,
    GEOLOC_VERY_POOR = 4
} GeolocationQuality_t;

int geoloc_build_google_query(CellData_t *cell_data, char *json_buf, uint32_t json_size);
int geoloc_build_here_query(CellData_t *cell_data, char *json_buf, uint32_t json_size);
int geoloc_encode_url_params(CellData_t *cell_data, char *url_buf, uint32_t url_size);
int geoloc_parse_response(const char *response, double *lat, double *lng, uint32_t *accuracy);
GeolocationQuality_t geoloc_estimate_quality(uint32_t accuracy_meters);
const char *geoloc_quality_string(GeolocationQuality_t quality);

#endif /* GEOLOC_H */

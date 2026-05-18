#ifndef __GEOLOC_H__
#define __GEOLOC_H__

#include <stdint.h>
#include "modem.h"

// Geolocation quality indicators
typedef enum {
    GEOLOC_EXCELLENT = 0,   // <500m
    GEOLOC_GOOD = 1,        // <1km
    GEOLOC_ACCEPTABLE = 2,  // <5km
    GEOLOC_POOR = 3,        // <10km
    GEOLOC_VERY_POOR = 4    // >10km
} GeolocationQuality_t;

// Geolocation result
typedef struct {
    double latitude;
    double longitude;
    uint32_t accuracy_meters;
    GeolocationQuality_t quality;
    const char *provider;  // "google" or "here"
} GeolocationResult_t;

// Build geolocation query for Google API
int geoloc_build_google_query(CellData_t *cell_data, char *json_buf, uint32_t json_size);

// Build geolocation query for HERE API
int geoloc_build_here_query(CellData_t *cell_data, char *json_buf, uint32_t json_size);

// URL encoding for NFC payload
int geoloc_encode_url_params(CellData_t *cell_data, char *url_buf, uint32_t url_size);

// Parse geolocation response JSON
int geoloc_parse_response(const char *response, double *lat, double *lng, uint32_t *accuracy);

// Estimate quality from accuracy
GeolocationQuality_t geoloc_estimate_quality(uint32_t accuracy_meters);
const char *geoloc_quality_string(GeolocationQuality_t quality);

#endif  // __GEOLOC_H__

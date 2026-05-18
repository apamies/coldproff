/*
 * geoloc.c — Cell ID → URL encoding for NFC payload (platform-independent)
 *
 * No platform dependencies: pure C stdlib only.
 */

#include "geoloc.h"
#include "coldproff_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "GEOLOC"

int geoloc_build_google_query(CellData_t *cell_data, char *json_buf, uint32_t json_size) {
    if (!cell_data || !json_buf || json_size < 200) return -1;
    int n = snprintf(json_buf, json_size,
        "{\"radioType\":\"lte\",\"cellTowers\":[{"
        "\"mobileCountryCode\":%d,"
        "\"mobileNetworkCode\":%d,"
        "\"locationAreaCode\":%d,"
        "\"cellId\":%u}]}",
        cell_data->mcc, cell_data->mnc,
        cell_data->tac, cell_data->cell_id);
    return (n < 0 || n >= (int)json_size) ? -1 : n;
}

int geoloc_build_here_query(CellData_t *cell_data, char *json_buf, uint32_t json_size) {
    if (!cell_data || !json_buf || json_size < 150) return -1;
    int n = snprintf(json_buf, json_size,
        "{\"lte\":[{\"mcc\":%d,\"mnc\":%d,\"cid\":%u}]}",
        cell_data->mcc, cell_data->mnc, cell_data->cell_id);
    return (n < 0 || n >= (int)json_size) ? -1 : n;
}

/*
 * URL parameters embedded in NDEF URI record.
 * Server reads: cid (hex), tac (hex), mcc, mnc, rsrp (dBm), n (reading index).
 * Server converts temp from raw using the 18-byte record in EEPROM.
 */
int geoloc_encode_url_params(CellData_t *cell_data, char *url_buf, uint32_t url_size) {
    if (!cell_data || !url_buf || url_size < 100) return -1;
    int n = snprintf(url_buf, url_size,
        "https://sensor.example.com/locate"
        "?cid=%X&tac=%X&mcc=%d&mnc=%d&rsrp=%d&n=%d",
        cell_data->cell_id, cell_data->tac,
        cell_data->mcc, cell_data->mnc,
        (int)cell_data->rsrp,
        cell_data->measurement_count);
    return (n < 0 || n >= (int)url_size) ? -1 : n;
}

int geoloc_parse_response(const char *response, double *lat, double *lng,
                          uint32_t *accuracy) {
    if (!response || !lat || !lng || !accuracy) return -1;
    float lat_v = 0, lng_v = 0;
    unsigned acc_v = 0;
    const char *p;

    p = strstr(response, "\"lat\"");
    if (!p) return -1;
    if (sscanf(p, "\"lat\" : %f", &lat_v) != 1 &&
        sscanf(p, "\"lat\":%f",   &lat_v) != 1) return -1;

    p = strstr(response, "\"lng\"");
    if (!p) return -1;
    if (sscanf(p, "\"lng\" : %f", &lng_v) != 1 &&
        sscanf(p, "\"lng\":%f",   &lng_v) != 1) return -1;

    p = strstr(response, "\"accuracy\"");
    if (!p) return -1;
    if (sscanf(p, "\"accuracy\" : %u", &acc_v) != 1 &&
        sscanf(p, "\"accuracy\":%u",   &acc_v) != 1) return -1;

    *lat = (double)lat_v;
    *lng = (double)lng_v;
    *accuracy = acc_v;
    return 0;
}

GeolocationQuality_t geoloc_estimate_quality(uint32_t accuracy_meters) {
    if (accuracy_meters <  500) return GEOLOC_EXCELLENT;
    if (accuracy_meters < 1000) return GEOLOC_GOOD;
    if (accuracy_meters < 5000) return GEOLOC_ACCEPTABLE;
    if (accuracy_meters <10000) return GEOLOC_POOR;
    return GEOLOC_VERY_POOR;
}

const char *geoloc_quality_string(GeolocationQuality_t q) {
    switch (q) {
        case GEOLOC_EXCELLENT:  return "Excellent (<500m)";
        case GEOLOC_GOOD:       return "Good (<1km)";
        case GEOLOC_ACCEPTABLE: return "Acceptable (<5km)";
        case GEOLOC_POOR:       return "Poor (<10km)";
        default:                return "Very Poor (>10km)";
    }
}

/*
 * Geolocation encoding - Build JSON queries for Google/HERE APIs
 * Sends data from ST25DV64K to server for geolocation lookup
 */

#include "ql_api_common.h"
#include "ql_log.h"
#include "geoloc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "GEOLOC"

/*
 * Build Google Geolocation API JSON payload
 *
 * Request format:
 * {
 *   "radioType": "lte",
 *   "cellTowers": [{
 *     "mobileCountryCode": 214,
 *     "mobileNetworkCode": 3,
 *     "locationAreaCode": 2250,
 *     "cellId": 110324069
 *   }]
 * }
 */
int geoloc_build_google_query(CellData_t *cell_data, char *json_buf, uint32_t json_size) {
    if (!cell_data || !json_buf || json_size < 200) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    // Determine RAT based on available data
    const char *radio_type = "lte";  // TODO: detect WCDMA vs LTE from cell_data

    int written = snprintf(
        json_buf, json_size,
        "{"
        "\"radioType\":\"%s\","
        "\"cellTowers\":[{"
        "\"mobileCountryCode\":%d,"
        "\"mobileNetworkCode\":%d,"
        "\"locationAreaCode\":%d,"
        "\"cellId\":%u"
        "}]"
        "}",
        radio_type,
        cell_data->mcc,
        cell_data->mnc,
        cell_data->tac,
        cell_data->cell_id
    );

    if (written < 0 || written >= (int)json_size) {
        QL_LOG_ERR(TAG, "JSON buffer overflow");
        return -1;
    }

    QL_LOG_DEBUG(TAG, "Google query: %s", json_buf);
    return written;
}

/*
 * Build HERE Positioning API v2 JSON payload
 *
 * Request format (LTE):
 * {
 *   "lte": [{
 *     "mcc": 214,
 *     "mnc": 3,
 *     "cid": 110324069
 *   }]
 * }
 *
 * Note: HERE LTE does NOT accept "lac" field
 */
int geoloc_build_here_query(CellData_t *cell_data, char *json_buf, uint32_t json_size) {
    if (!cell_data || !json_buf || json_size < 150) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    int written = snprintf(
        json_buf, json_size,
        "{"
        "\"lte\":[{"
        "\"mcc\":%d,"
        "\"mnc\":%d,"
        "\"cid\":%u"
        "}]"
        "}",
        cell_data->mcc,
        cell_data->mnc,
        cell_data->cell_id
    );

    if (written < 0 || written >= (int)json_size) {
        QL_LOG_ERR(TAG, "JSON buffer overflow");
        return -1;
    }

    QL_LOG_DEBUG(TAG, "HERE query: %s", json_buf);
    return written;
}

/*
 * URL-encode geolocation data for NFC payload.
 * cid/tac in hex (as parsed from AT+QENG), mcc/mnc in decimal, rsrp in dBm.
 * Format: https://sensor.example.com/locate?cid=XXXXX&tac=XXXX&mcc=XXX&mnc=XX&rsrp=-XX
 */
int geoloc_encode_url_params(CellData_t *cell_data, char *url_buf, uint32_t url_size) {
    if (!cell_data || !url_buf || url_size < 100) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    int written = snprintf(
        url_buf, url_size,
        "https://sensor.example.com/locate?cid=%X&tac=%X&mcc=%d&mnc=%d&rsrp=%d",
        cell_data->cell_id,
        cell_data->tac,
        cell_data->mcc,
        cell_data->mnc,
        (int)cell_data->rsrp
    );

    if (written < 0 || written >= (int)url_size) {
        QL_LOG_ERR(TAG, "URL buffer overflow");
        return -1;
    }

    QL_LOG_DEBUG(TAG, "URL: %s", url_buf);
    return written;
}

/*
 * Parse geolocation response from server.
 * Google response example:
 *   {"location": {"lat": 41.2341, "lng": 1.2341}, "accuracy": 337}
 *
 * Uses strstr to locate keys — tolerates whitespace and key ordering.
 * Does NOT require a full JSON parser.
 */
int geoloc_parse_response(const char *response, double *lat, double *lng, uint32_t *accuracy) {
    if (!response || !lat || !lng || !accuracy) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    float lat_val = 0.0f, lng_val = 0.0f;
    unsigned int acc_val = 0;

    const char *p;

    p = strstr(response, "\"lat\"");
    if (!p || sscanf(p, "\"lat\" : %f", &lat_val) != 1) {
        if (!p || sscanf(p, "\"lat\":%f", &lat_val) != 1) {
            QL_LOG_ERR(TAG, "\"lat\" not found in response");
            return -1;
        }
    }

    p = strstr(response, "\"lng\"");
    if (!p || sscanf(p, "\"lng\" : %f", &lng_val) != 1) {
        if (!p || sscanf(p, "\"lng\":%f", &lng_val) != 1) {
            QL_LOG_ERR(TAG, "\"lng\" not found in response");
            return -1;
        }
    }

    p = strstr(response, "\"accuracy\"");
    if (!p || sscanf(p, "\"accuracy\" : %u", &acc_val) != 1) {
        if (!p || sscanf(p, "\"accuracy\":%u", &acc_val) != 1) {
            QL_LOG_ERR(TAG, "\"accuracy\" not found in response");
            return -1;
        }
    }

    *lat      = (double)lat_val;
    *lng      = (double)lng_val;
    *accuracy = acc_val;

    QL_LOG_INFO(TAG, "Location: %.4f, %.4f ± %u m", *lat, *lng, *accuracy);
    return 0;
}

/*
 * Estimate geolocation accuracy category
 */
GeolocationQuality_t geoloc_estimate_quality(uint32_t accuracy_meters) {
    if (accuracy_meters < 500) {
        return GEOLOC_EXCELLENT;  // <500m
    } else if (accuracy_meters < 1000) {
        return GEOLOC_GOOD;        // <1km
    } else if (accuracy_meters < 5000) {
        return GEOLOC_ACCEPTABLE;  // <5km
    } else if (accuracy_meters < 10000) {
        return GEOLOC_POOR;        // <10km
    } else {
        return GEOLOC_VERY_POOR;   // >10km
    }
}

const char *geoloc_quality_string(GeolocationQuality_t quality) {
    switch (quality) {
        case GEOLOC_EXCELLENT:  return "Excellent (<500m)";
        case GEOLOC_GOOD:       return "Good (<1km)";
        case GEOLOC_ACCEPTABLE: return "Acceptable (<5km)";
        case GEOLOC_POOR:       return "Poor (<10km)";
        case GEOLOC_VERY_POOR:  return "Very Poor (>10km)";
        default:                return "Unknown";
    }
}

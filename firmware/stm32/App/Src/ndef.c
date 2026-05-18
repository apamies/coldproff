/*
 * ndef.c — NDEF Type 4 Tag encoding for ST25DV64K (platform-independent)
 *
 * TLV layout in EEPROM: [0x03][len][NDEF message bytes][0xFE]
 * No platform dependencies beyond string.h and stdio.h.
 */

#include "ndef.h"
#include "geoloc.h"
#include "coldproff_log.h"
#include <string.h>
#include <stdio.h>

#define TAG "NDEF"

#define NDEF_MB              0x80
#define NDEF_ME              0x40
#define NDEF_SR              0x10
#define NDEF_TNF_WELL_KNOWN  0x01

/* ------------------------------------------------------------------ */
int ndef_build_url_record(const char *url, uint8_t *buf, uint32_t buf_size) {
    if (!url || !buf || buf_size < 10) return -1;

    uint32_t url_len     = strlen(url);
    uint32_t payload_len = url_len + 1;   /* URI code byte + URL */
    if (payload_len > 255) return -1;

    uint32_t total = 4 + payload_len;
    if (total > buf_size) return -1;

    uint32_t i = 0;
    buf[i++] = NDEF_MB | NDEF_ME | NDEF_SR | NDEF_TNF_WELL_KNOWN;
    buf[i++] = 0x01;                    /* type length: 1 ('U') */
    buf[i++] = (uint8_t)payload_len;
    buf[i++] = 'U';
    buf[i++] = 0x00;                    /* URI identifier: no prefix, full URL */
    memcpy(&buf[i], url, url_len);
    i += url_len;

    LOG_D(TAG, "URL record %u bytes: %s", i, url);
    return (int)i;
}

/* ------------------------------------------------------------------ */
int ndef_build_text_record(const char *text, uint8_t *buf, uint32_t buf_size) {
    if (!text || !buf || buf_size < 10) return -1;

    uint32_t text_len    = strlen(text);
    uint32_t payload_len = 3 + text_len;  /* status + "en" + text */
    if (payload_len > 255) return -1;

    uint32_t total = 4 + payload_len;
    if (total > buf_size) return -1;

    uint32_t i = 0;
    buf[i++] = NDEF_MB | NDEF_ME | NDEF_SR | NDEF_TNF_WELL_KNOWN;
    buf[i++] = 0x01;                     /* type length: 1 ('T') */
    buf[i++] = (uint8_t)payload_len;
    buf[i++] = 'T';
    buf[i++] = 0x02;                     /* UTF-8, language code length = 2 */
    buf[i++] = 'e';
    buf[i++] = 'n';
    memcpy(&buf[i], text, text_len);
    i += text_len;

    return (int)i;
}

/* ------------------------------------------------------------------ */
int ndef_build_sensor_record(CellData_t *cell_data, uint8_t *buf, uint32_t buf_size) {
    if (!cell_data || !buf || buf_size < 300) return -1;

    char url[256];
    if (geoloc_encode_url_params(cell_data, url, sizeof(url)) < 0) return -1;

    /* Record 1: URL (MB=1, ME=0) */
    int url_len = ndef_build_url_record(url, buf, buf_size);
    if (url_len < 0) return -1;
    buf[0] &= ~NDEF_ME;     /* not the last record */

    uint32_t off = (uint32_t)url_len;

    /* Record 2: text metadata (MB=0, ME=1) */
    char meta[96];
    snprintf(meta, sizeof(meta), "CID:%lX TAC:%X RSRP:%d",
             (unsigned long)cell_data->cell_id, cell_data->tac,
             (int)cell_data->rsrp);

    if (off >= buf_size) return -1;
    int text_len = ndef_build_text_record(meta, &buf[off], buf_size - off);
    if (text_len < 0) return -1;
    buf[off] &= ~NDEF_MB;
    buf[off] |=  NDEF_ME;
    off += (uint32_t)text_len;

    LOG_I(TAG, "Sensor NDEF %u bytes, url=%s", off, url);
    return (int)off;
}

/* ------------------------------------------------------------------ */
int ndef_write_to_eeprom(const uint8_t *ndef_data, uint32_t ndef_len,
                         int (*write_fn)(uint16_t addr, uint8_t *data, uint32_t len)) {
    if (!ndef_data || !write_fn || ndef_len == 0 || ndef_len > 8000) return -1;

    /* Build TLV header */
    uint8_t  header[4];
    uint32_t header_len;
    uint16_t addr = 0x0000;

    if (ndef_len <= 254) {
        header[0] = NDEF_TLV_TYPE;
        header[1] = (uint8_t)ndef_len;
        header_len = 2;
    } else {
        header[0] = NDEF_TLV_TYPE;
        header[1] = 0xFF;
        header[2] = (uint8_t)(ndef_len >> 8);
        header[3] = (uint8_t)(ndef_len & 0xFF);
        header_len = 4;
    }

    if (write_fn(addr, header, header_len) != 0) return -1;
    addr += (uint16_t)header_len;

    /* Write NDEF payload in 64-byte chunks */
    uint32_t rem = ndef_len, off = 0;
    while (rem > 0) {
        uint32_t chunk = rem > 64 ? 64 : rem;
        if (write_fn(addr, (uint8_t *)&ndef_data[off], chunk) != 0) return -1;
        addr += (uint16_t)chunk;
        off  += chunk;
        rem  -= chunk;
    }

    /* Terminator TLV */
    uint8_t term = NDEF_TLV_TERMINATOR;
    if (write_fn(addr, &term, 1) != 0) return -1;

    LOG_I(TAG, "NDEF written: %u payload + %u header bytes", ndef_len, header_len);
    return 0;
}

/* ------------------------------------------------------------------ */
int ndef_parse_message(const uint8_t *ndef_buf, uint32_t buf_len,
                       char *url_out, uint32_t url_size) {
    if (!ndef_buf || !url_out || buf_len < 4 || url_size < 2) return -1;
    if (ndef_buf[0] != NDEF_TLV_TYPE) return -1;

    uint32_t msg_len, msg_off;
    if (ndef_buf[1] == 0xFF) {
        if (buf_len < 5) return -1;
        msg_len = ((uint32_t)ndef_buf[2] << 8) | ndef_buf[3];
        msg_off = 4;
    } else {
        msg_len = ndef_buf[1];
        msg_off = 2;
    }
    if (msg_len == 0 || msg_off + msg_len > buf_len) return -1;

    const uint8_t *rec = ndef_buf + msg_off;
    const uint8_t *end = rec + msg_len;

    while (rec < end) {
        if (rec + 3 > end) break;
        uint8_t type_len    = rec[1];
        uint8_t payload_len = rec[2];
        uint32_t overhead   = 3 + type_len;
        if (rec + overhead + payload_len > end) break;

        if ((rec[0] & 0x07) == NDEF_TNF_WELL_KNOWN &&
            type_len == 1 && rec[3] == 'U' && payload_len >= 2) {
            uint32_t url_len = payload_len - 1;
            if (url_len >= url_size) return -1;
            memcpy(url_out, &rec[5], url_len);
            url_out[url_len] = '\0';
            return (int)url_len;
        }
        rec += overhead + payload_len;
    }
    return -1;
}

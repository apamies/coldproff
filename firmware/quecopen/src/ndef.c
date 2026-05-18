/*
 * NDEF Type 4 Tag encoding for ST25DV64K
 * Formats NDEF messages and writes them to EEPROM in TLV layout:
 *   [0x03][length][NDEF message bytes...][0xFE]
 */

#include "ql_api_common.h"
#include "ql_log.h"
#include "ndef.h"
#include "geoloc.h"
#include <string.h>
#include <stdio.h>

#define TAG "NDEF"

// NFC Forum NDEF record header flags
#define NDEF_MB   0x80   // Message Begin
#define NDEF_ME   0x40   // Message End
#define NDEF_SR   0x10   // Short Record (payload length in 1 byte)
#define NDEF_TNF_WELL_KNOWN 0x01

/*
 * Build NDEF URI record (NFC Forum URI RTD)
 *
 * Wire format (Short Record):
 *   [Header] [TypeLen=1] [PayloadLen] ['U'] [URIcode=0x00] [URL bytes]
 *
 * Header = MB | ME | SR | TNF_WELL_KNOWN
 * URI code 0x00 = no prefix abbreviation (full URL in payload)
 *
 * Returns bytes written, -1 on error.
 */
int ndef_build_url_record(const char *url, uint8_t *ndef_buf, uint32_t buf_size) {
    if (!url || !ndef_buf || buf_size < 10) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    uint32_t url_len = strlen(url);
    uint32_t payload_len = url_len + 1;  // URI code byte + URL

    if (payload_len > 255) {
        QL_LOG_ERR(TAG, "URL too long for Short Record");
        return -1;
    }

    uint32_t total = 4 + payload_len;  // header + type_len + payload_len + type + payload
    if (total > buf_size) {
        QL_LOG_ERR(TAG, "Buffer too small (%u needed, %u available)", total, buf_size);
        return -1;
    }

    uint32_t i = 0;
    ndef_buf[i++] = NDEF_MB | NDEF_ME | NDEF_SR | NDEF_TNF_WELL_KNOWN;
    ndef_buf[i++] = 0x01;                    // Type length: 1 byte ('U')
    ndef_buf[i++] = (uint8_t)payload_len;    // Payload length
    ndef_buf[i++] = 'U';                     // Type: URI RTD
    ndef_buf[i++] = 0x00;                    // URI code: no prefix, full URL
    memcpy(&ndef_buf[i], url, url_len);
    i += url_len;

    QL_LOG_DEBUG(TAG, "URL record: %u bytes, url=%s", i, url);
    return (int)i;
}

/*
 * Build NDEF Text record (NFC Forum Text RTD)
 *
 * Wire format (Short Record):
 *   [Header] [TypeLen=1] [PayloadLen] ['T'] [StatusByte] ['e']['n'] [text bytes]
 *
 * StatusByte = 0x02 (UTF-8, language code length = 2)
 *
 * Returns bytes written, -1 on error.
 */
int ndef_build_text_record(const char *text, uint8_t *ndef_buf, uint32_t buf_size) {
    if (!text || !ndef_buf || buf_size < 10) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    uint32_t text_len = strlen(text);
    uint32_t payload_len = 3 + text_len;  // status byte + "en" + text

    if (payload_len > 255) {
        QL_LOG_ERR(TAG, "Text too long for Short Record");
        return -1;
    }

    uint32_t total = 4 + payload_len;
    if (total > buf_size) {
        QL_LOG_ERR(TAG, "Buffer too small (%u needed, %u available)", total, buf_size);
        return -1;
    }

    uint32_t i = 0;
    ndef_buf[i++] = NDEF_MB | NDEF_ME | NDEF_SR | NDEF_TNF_WELL_KNOWN;
    ndef_buf[i++] = 0x01;                     // Type length: 1 byte ('T')
    ndef_buf[i++] = (uint8_t)payload_len;     // Payload length
    ndef_buf[i++] = 'T';                      // Type: Text RTD
    ndef_buf[i++] = 0x02;                     // Status: UTF-8, lang code = 2 chars
    ndef_buf[i++] = 'e';
    ndef_buf[i++] = 'n';
    memcpy(&ndef_buf[i], text, text_len);
    i += text_len;

    QL_LOG_DEBUG(TAG, "Text record: %u bytes", i);
    return (int)i;
}

/*
 * Build NDEF message with two records: URL (geolocation) + Text (metadata)
 * Multi-record: first record has MB only, last has ME only.
 *
 * Returns total bytes, -1 on error.
 */
int ndef_build_sensor_record(CellData_t *cell_data, uint8_t *ndef_buf, uint32_t buf_size) {
    if (!cell_data || !ndef_buf || buf_size < 300) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    char url[256];
    if (geoloc_encode_url_params(cell_data, url, sizeof(url)) < 0) {
        QL_LOG_ERR(TAG, "Failed to encode URL");
        return -1;
    }

    // Record 1: URL (MB=1, ME=0)
    int url_rec_len = ndef_build_url_record(url, ndef_buf, buf_size);
    if (url_rec_len < 0) {
        QL_LOG_ERR(TAG, "Failed to build URL record");
        return -1;
    }
    ndef_buf[0] &= ~NDEF_ME;  // Clear ME: this is not the last record

    uint32_t offset = (uint32_t)url_rec_len;

    // Record 2: Text metadata (MB=0, ME=1)
    char metadata[128];
    snprintf(metadata, sizeof(metadata),
             "CID:%X TAC:%X RSRP:%d",
             cell_data->cell_id, cell_data->tac, (int)cell_data->rsrp);

    if (offset >= buf_size) {
        QL_LOG_ERR(TAG, "Buffer overflow before text record");
        return -1;
    }

    int text_rec_len = ndef_build_text_record(metadata, &ndef_buf[offset], buf_size - offset);
    if (text_rec_len < 0) {
        QL_LOG_ERR(TAG, "Failed to build text record");
        return -1;
    }

    // Adjust flags: record 2 must have ME=1, MB=0
    ndef_buf[offset] &= ~NDEF_MB;
    ndef_buf[offset] |=  NDEF_ME;

    offset += (uint32_t)text_rec_len;

    QL_LOG_INFO(TAG, "Sensor NDEF built: %u bytes, url=%s", offset, url);
    return (int)offset;
}

/*
 * Write NDEF message to ST25DV64K EEPROM in TLV format:
 *   Byte 0:       0x03 (NDEF Message TLV type)
 *   Byte 1:       length (if <= 254)
 *   Byte 1-3:     0xFF + length MSB + LSB (if > 254)
 *   Byte 2 (4):   NDEF message
 *   Last byte:    0xFE (Terminator TLV)
 *
 * Writes start at EEPROM address 0x0000 (NDEF area).
 * Writes in 4-byte-aligned chunks to respect ST25DV page boundaries.
 */
int ndef_write_to_eeprom(const uint8_t *ndef_data, uint32_t ndef_len,
                         int (*i2c_write_fn)(uint16_t addr, uint8_t *data, uint32_t len)) {
    if (!ndef_data || !i2c_write_fn || ndef_len == 0 || ndef_len > 8000) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    // Build TLV header: [0x03][length...][NDEF data...][0xFE]
    // Use a small staging buffer for the header + terminator only;
    // write payload in chunks straight from ndef_data.
    uint8_t header[4];
    uint32_t header_len;
    uint16_t eeprom_addr = 0x0000;

    if (ndef_len <= 254) {
        header[0] = NDEF_TLV_TYPE;
        header[1] = (uint8_t)ndef_len;
        header_len = 2;
    } else {
        header[0] = NDEF_TLV_TYPE;
        header[1] = 0xFF;
        header[2] = (uint8_t)((ndef_len >> 8) & 0xFF);
        header[3] = (uint8_t)(ndef_len & 0xFF);
        header_len = 4;
    }

    // Write TLV header
    if (i2c_write_fn(eeprom_addr, header, header_len) != 0) {
        QL_LOG_ERR(TAG, "Failed to write TLV header");
        return -1;
    }
    eeprom_addr += (uint16_t)header_len;

    // Write NDEF payload in 64-byte chunks (ST25DV page boundary safe)
    uint32_t remaining = ndef_len;
    uint32_t written_so_far = 0;

    while (remaining > 0) {
        uint32_t chunk = (remaining > 64) ? 64 : remaining;

        if (i2c_write_fn(eeprom_addr, (uint8_t *)&ndef_data[written_so_far], chunk) != 0) {
            QL_LOG_ERR(TAG, "Failed to write NDEF payload at 0x%04X", eeprom_addr);
            return -1;
        }

        eeprom_addr    += (uint16_t)chunk;
        written_so_far += chunk;
        remaining      -= chunk;
    }

    // Write Terminator TLV
    uint8_t terminator = NDEF_TLV_TERMINATOR;
    if (i2c_write_fn(eeprom_addr, &terminator, 1) != 0) {
        QL_LOG_ERR(TAG, "Failed to write TLV terminator");
        return -1;
    }

    QL_LOG_INFO(TAG, "NDEF written to EEPROM: %u bytes payload, %u header",
                ndef_len, header_len);
    return 0;
}

/*
 * Parse NDEF URL from a TLV-wrapped EEPROM buffer.
 * Looks for the first URI record ('U' type) in the NDEF message.
 * Returns URL length, -1 on error.
 */
int ndef_parse_message(const uint8_t *ndef_buf, uint32_t buf_len,
                       char *url_out, uint32_t url_size) {
    if (!ndef_buf || !url_out || buf_len < 4 || url_size < 2) {
        QL_LOG_ERR(TAG, "Invalid arguments");
        return -1;
    }

    // Check NDEF Message TLV tag
    if (ndef_buf[0] != NDEF_TLV_TYPE) {
        QL_LOG_ERR(TAG, "Not an NDEF TLV block (got 0x%02X)", ndef_buf[0]);
        return -1;
    }

    uint32_t msg_len;
    uint32_t msg_offset;

    if (ndef_buf[1] == 0xFF) {
        // 3-byte length encoding
        if (buf_len < 5) {
            QL_LOG_ERR(TAG, "Buffer too small for 3-byte length");
            return -1;
        }
        msg_len    = ((uint32_t)ndef_buf[2] << 8) | ndef_buf[3];
        msg_offset = 4;
    } else {
        msg_len    = ndef_buf[1];
        msg_offset = 2;
    }

    if (msg_len == 0 || msg_offset + msg_len > buf_len) {
        QL_LOG_ERR(TAG, "Invalid NDEF length %u", msg_len);
        return -1;
    }

    // Walk NDEF records looking for the URI record (type 'U')
    const uint8_t *rec = &ndef_buf[msg_offset];
    const uint8_t *end = rec + msg_len;

    while (rec < end) {
        if (rec + 3 > end) break;  // truncated record

        uint8_t header      = rec[0];
        uint8_t type_len    = rec[1];
        uint8_t payload_len = rec[2];  // Short Record assumed (SR flag)
        uint32_t rec_overhead = 3 + type_len;

        if (rec + rec_overhead + payload_len > end) break;

        // Check for URI record (TNF=0x01, type='U')
        if ((header & 0x07) == NDEF_TNF_WELL_KNOWN &&
            type_len == 1 &&
            rec[3] == 'U' &&
            payload_len >= 2) {

            // URI code byte at rec[4], URL at rec[5..]
            uint32_t url_len = payload_len - 1;  // subtract URI code byte
            if (url_len >= url_size) {
                QL_LOG_ERR(TAG, "URL buffer too small");
                return -1;
            }

            memcpy(url_out, &rec[5], url_len);
            url_out[url_len] = '\0';

            QL_LOG_DEBUG(TAG, "Parsed URL (%u bytes): %s", url_len, url_out);
            return (int)url_len;
        }

        // Advance to next record
        rec += rec_overhead + payload_len;
    }

    QL_LOG_ERR(TAG, "No URI record found in NDEF message");
    return -1;
}

/*
 * Modem interface - AT command wrapper for BC660K-GL
 * Handles:
 * - QENG (serving cell info)
 * - CEREG (registration status)
 * - QPSMS (PSM mode)
 * - UART communication
 */

#include "ql_api_common.h"
#include "ql_log.h"
#include "ql_uart.h"
#include "modem.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "MODEM"

static ql_uart_t uart_dev = NULL;

// UART buffer
#define UART_RX_BUF_SIZE 2048
static uint8_t uart_rx_buf[UART_RX_BUF_SIZE];

typedef struct {
    uint8_t *data;
    uint32_t len;
} UartRxData_t;

// UART RX callback
static void uart_rx_callback(uint32_t port, void *param) {
    UartRxData_t *rx_data = (UartRxData_t *)param;
    uint32_t rx_len = ql_uart_read(uart_dev, rx_data->data, UART_RX_BUF_SIZE);
    rx_data->len = rx_len;
}

// Send AT command and wait for response
static int at_cmd_send(const char *cmd, char *response, uint32_t resp_size, uint32_t timeout_ms) {
    if (!uart_dev) {
        QL_LOG_ERR(TAG, "UART not initialized");
        return -1;
    }

    QL_LOG_DEBUG(TAG, "AT> %s", cmd);

    // Flush RX buffer
    ql_uart_read(uart_dev, uart_rx_buf, UART_RX_BUF_SIZE);

    // Send command
    char cmd_buf[256];
    snprintf(cmd_buf, sizeof(cmd_buf), "%s\r\n", cmd);
    ql_uart_write(uart_dev, (uint8_t *)cmd_buf, strlen(cmd_buf));

    // Wait for response (simple polling, no callback in this version)
    uint32_t start_time = ql_time_get_ms();
    uint32_t rx_count = 0;

    memset(response, 0, resp_size);

    while (ql_time_get_ms() - start_time < timeout_ms) {
        uint32_t rx_len = ql_uart_read(uart_dev, uart_rx_buf, UART_RX_BUF_SIZE);
        if (rx_len > 0) {
            if (rx_count + rx_len < resp_size) {
                memcpy(response + rx_count, uart_rx_buf, rx_len);
                rx_count += rx_len;
            }

            // Check for final markers
            if (strstr(response, "OK") || strstr(response, "ERROR")) {
                break;
            }
        }
        ql_rtos_task_sleep_ms(10);
    }

    response[resp_size - 1] = '\0';
    QL_LOG_DEBUG(TAG, "Response: %s", response);

    return (strstr(response, "ERROR") != NULL) ? -1 : 0;
}

int modem_init(void) {
    QL_LOG_INFO(TAG, "Initializing modem...");

    // Open UART for AT commands (typically UART0 or UART2 on BC660K-GL)
    // Configuration: 115200 baud, 8N1
    uart_dev = ql_uart_open(2, 115200, 0);  // UART2 is common for AT in Quectel
    if (!uart_dev) {
        QL_LOG_ERR(TAG, "Failed to open UART2");
        return -1;
    }

    QL_LOG_INFO(TAG, "UART opened (115200 baud)");

    // Test with ATI (module identification)
    char response[256];
    if (at_cmd_send("ATI", response, sizeof(response), 2000) == 0) {
        QL_LOG_INFO(TAG, "Module: %s", response);
    } else {
        QL_LOG_ERR(TAG, "Module identification failed");
        return -1;
    }

    // Disable echo for cleaner parsing
    at_cmd_send("ATE0", response, sizeof(response), 1000);

    // Enable +CEREG unsolicited notifications
    at_cmd_send("AT+CEREG=2", response, sizeof(response), 1000);

    return 0;
}

int modem_check_registration(void) {
    char response[256];
    int n, stat;

    if (at_cmd_send("AT+CEREG?", response, sizeof(response), 2000) != 0) {
        QL_LOG_ERR(TAG, "CEREG query failed");
        return -1;
    }

    // Parse: +CEREG: <n>,<stat>[,<lac>,<ci>[,<AcT>]]
    // stat: 0=no, 1=home, 2=search, 3=denied, 4=unknown, 5=roaming, 8=LIMSRV
    n = sscanf(response, "+CEREG: %*d,%d", &stat);
    if (n != 1) {
        QL_LOG_ERR(TAG, "Failed to parse CEREG");
        return -1;
    }

    return stat;
}

int modem_read_serving_cell(CellData_t *cell_data) {
    char response[512];

    if (at_cmd_send("AT+QENG=\"servingcell\"", response, sizeof(response), 5000) != 0) {
        QL_LOG_ERR(TAG, "QENG command failed");
        return -1;
    }

    // Response (LTE FDD example):
    // +QENG: "servingcell","FDD",214,03,6936965,235,3050,7,5,5,8CA,18,-98,-11,-72,7,59,5
    //  tok:  [0]           [1]   [2] [3][4]      [5] [6]  ...     [10] [11][12]
    //  field:              RAT   MCC MNC CID      ...             TAC       RSRP

    // Tokenize on commas into a flat array
    char buf[512];
    strncpy(buf, response, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok[25];
    int   ntok = 0;
    char *p = buf;
    char *t = strtok(p, ",");
    while (t && ntok < 25) {
        // strip leading/trailing spaces and quotes
        while (*t == ' ' || *t == '"') t++;
        char *end = t + strlen(t) - 1;
        while (end > t && (*end == ' ' || *end == '"' || *end == '\r' || *end == '\n')) *end-- = '\0';
        tok[ntok++] = t;
        t = strtok(NULL, ",");
    }

    // Need at least 13 tokens: [0]=+QENG: "servingcell" [1]=RAT [2]=MCC [3]=MNC
    //   [4]=CID [5..9]=skip [10]=TAC [11]=skip [12]=RSRP
    if (ntok < 13) {
        QL_LOG_ERR(TAG, "QENG: too few tokens (%d)", ntok);
        QL_LOG_DEBUG(TAG, "Raw: %s", response);
        return -1;
    }

    // CRITICAL: CID and TAC are hexadecimal strings (Quectel reports in hex)
    cell_data->mcc     = (uint16_t)atoi(tok[2]);
    cell_data->mnc     = (uint16_t)atoi(tok[3]);
    cell_data->cell_id = (uint32_t)strtoul(tok[4], NULL, 16);
    cell_data->tac     = (uint16_t)strtoul(tok[10], NULL, 16);
    cell_data->rsrp    = (int16_t)atoi(tok[12]);

    QL_LOG_INFO(TAG, "QENG: MCC=%d MNC=%d CID=0x%X TAC=0x%X RSRP=%d dBm",
                cell_data->mcc, cell_data->mnc,
                cell_data->cell_id, cell_data->tac,
                (int)cell_data->rsrp);
    return 0;
}

// Convert UTC date+time components to Unix timestamp (seconds since 1970-01-01 00:00:00 UTC).
// No dependency on system timezone or mktime().
static uint32_t utc_to_unix(int year, int mon, int day, int h, int m, int s) {
    static const uint8_t dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t days = 0;
    int y;

    for (y = 1970; y < year; y++)
        days += ((y % 4 == 0) && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;

    int leap = ((year % 4 == 0) && (year % 100 != 0 || year % 400 == 0));
    for (int mo = 1; mo < mon; mo++) {
        days += dim[mo - 1];
        if (mo == 2 && leap) days++;
    }
    days += day - 1;

    return days * 86400U + (uint32_t)h * 3600 + (uint32_t)m * 60 + (uint32_t)s;
}

/*
 * Get current UTC Unix timestamp from the modem.
 *
 * Strategy:
 *   1. AT+QLTS=1 — LTE network time (SIB16, synced from tower, no SIM needed)
 *   2. AT+CCLK?  — internal RTC fallback (accurate only if previously synced)
 *
 * Sets *ts_out = 0 if time is unavailable or obviously invalid (year < 2025).
 * A zero timestamp signals the server to use reading-index extrapolation instead.
 *
 * Response format: +QLTS: "YY/MM/DD,HH:MM:SS±ZZ,DST"
 *   ZZ = timezone offset in quarter-hours (signed, e.g. +04 = UTC+1 for Spain CET)
 */
int modem_get_unix_time(uint32_t *ts_out) {
    char response[128];
    *ts_out = 0;

    // Try network time first (available in LIMSRV mode via LTE SIB16)
    int ok = at_cmd_send("AT+QLTS=1", response, sizeof(response), 2000);
    if (ok != 0) {
        // Fallback: internal RTC
        ok = at_cmd_send("AT+CCLK?", response, sizeof(response), 1000);
        if (ok != 0) {
            QL_LOG_ERR(TAG, "Time unavailable (QLTS and CCLK failed)");
            return -1;
        }
    }

    // Find the quoted time string in the response
    char *q = strchr(response, '"');
    if (!q) {
        QL_LOG_ERR(TAG, "Time parse error: no quote found in: %s", response);
        return -1;
    }
    q++;  // skip opening quote

    int yy, mo, dd, hh, mm, ss, tz_qh = 0;
    char tz_sign = '+';

    // Parse: "YY/MM/DD,HH:MM:SS±ZZ,..."
    int n = sscanf(q, "%d/%d/%d,%d:%d:%d%c%d",
                   &yy, &mo, &dd, &hh, &mm, &ss, &tz_sign, &tz_qh);
    if (n < 6) {
        QL_LOG_ERR(TAG, "Time parse error (got %d fields): %s", n, q);
        return -1;
    }

    int year = 2000 + yy;
    if (year < 2025) {
        // RTC not initialized — modem booted without network time
        QL_LOG_INFO(TAG, "Time invalid (year %d < 2025) — timestamp will be 0", year);
        return 0;
    }

    // Convert local time to UTC: subtract timezone offset
    uint32_t unix_local = utc_to_unix(year, mo, dd, hh, mm, ss);
    int tz_seconds = tz_qh * 15 * 60;
    uint32_t unix_utc = (tz_sign == '+')
                        ? unix_local - (uint32_t)tz_seconds
                        : unix_local + (uint32_t)tz_seconds;

    *ts_out = unix_utc;
    QL_LOG_INFO(TAG, "Time: %04d-%02d-%02d %02d:%02d:%02d UTC+%c%d/4 → Unix %lu",
                year, mo, dd, hh, mm, ss, tz_sign, tz_qh, (unsigned long)unix_utc);
    return 0;
}

int modem_enter_psm(uint32_t sleep_seconds) {
    char response[256];
    char cmd[64];

    // AT+QPSMS=<enable>,<tau_timer>,<active_timer>
    // tau_timer: T3412 extended timer (in units)
    // For 3 hours: 10800 seconds
    // Quectel format: T3412 value (varies by device, typically in 10min units)

    // Enable PSM with ~3 hour sleep
    // Format: 3 hours = ~18 units of 10min (3*60/10)
    int tau_units = (sleep_seconds / 600) + 1;  // Convert to 10-min units, round up

    snprintf(cmd, sizeof(cmd), "AT+QPSMS=1,,,%d", tau_units);
    if (at_cmd_send(cmd, response, sizeof(response), 2000) != 0) {
        QL_LOG_ERR(TAG, "PSM enable failed");
        return -1;
    }

    QL_LOG_INFO(TAG, "PSM enabled (%d seconds)", sleep_seconds);
    return 0;
}

int modem_exit_psm(void) {
    char response[256];
    if (at_cmd_send("AT+QPSMS=0", response, sizeof(response), 2000) != 0) {
        QL_LOG_ERR(TAG, "PSM disable failed");
        return -1;
    }

    QL_LOG_INFO(TAG, "PSM disabled");
    return 0;
}

void modem_close(void) {
    if (uart_dev) {
        ql_uart_close(uart_dev);
        uart_dev = NULL;
    }
}

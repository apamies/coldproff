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
    int rat;
    char rat_str[16];
    int earfcn, pci, rsrp_val;

    if (at_cmd_send("AT+QENG=\"servingcell\"", response, sizeof(response), 5000) != 0) {
        QL_LOG_ERR(TAG, "QENG command failed");
        return -1;
    }

    // Parse response: +QENG: "servingcell","FDD",mcc,mnc,cid,pcid,earfcn,...,lac,...
    // Example: +QENG: "servingcell","FDD",214,03,6936965,235,3050,7,5,5,8CA,...
    //
    // Fields (0-indexed):
    // 0: "servingcell"
    // 1: "FDD" (or other RAT)
    // 2: MCC (214 = Spain)
    // 3: MNC (03 = Orange)
    // 4: CID (hex string "6936965")
    // 5: PCID (decimal)
    // 6: EARFCN (decimal)
    // 12: LAC/TAC (hex string "8CA")

    int mcc, mnc;
    char cid_hex[16], tac_hex[16];

    // Simple parser (robust version should use regex or proper tokenizer)
    int n = sscanf(response,
                   "+QENG: \"servingcell\",\"%15[^\"]\"%*[^,],%d,%d,%15[^,],%d,%d,%*[^,],%*[^,],%*[^,],%15[^,]",
                   rat_str, &mcc, &mnc, cid_hex, &pci, &earfcn, tac_hex);

    if (n < 5) {
        QL_LOG_ERR(TAG, "Failed to parse QENG (parsed %d fields)", n);
        QL_LOG_DEBUG(TAG, "Raw response: %s", response);
        return -1;
    }

    // CRITICAL: Cell IDs are HEXADECIMAL (even if only contain 0-9)
    cell_data->cell_id = strtoul(cid_hex, NULL, 16);
    cell_data->tac = strtoul(tac_hex, NULL, 16);
    cell_data->mcc = mcc;
    cell_data->mnc = mnc;
    cell_data->rsrp = rsrp_val;  // TODO: extract from full QENG response

    QL_LOG_INFO(TAG, "Parsed QENG: MCC=%d MNC=%d CID=0x%X TAC=0x%X",
               mcc, mnc, cell_data->cell_id, cell_data->tac);

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

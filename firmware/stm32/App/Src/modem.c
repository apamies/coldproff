/*
 * modem.c — BC65 NB-IoT driver for STM32L010F4P6
 *
 * UART: USART2 at 9600 baud (BC65 default), upgraded to 115200 after init.
 * Power control: PWRKEY pulse ≥650 ms toggles BC65 on/off.
 * Cell data: AT+QENG="servingcell" — CID and TAC are ALWAYS hex strings.
 * Time: AT+QLTS=1 (LTE SIB16) → AT+CCLK? (internal RTC fallback).
 */

#include "modem.h"
#include "coldproff_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "MODEM"

#define UART_BUF_SIZE  512
#define PWRKEY_PULSE_MS 700     /* BC65 datasheet: >650 ms */
#define POWER_ON_TIMEOUT_MS 10000

static UART_HandleTypeDef *huart = NULL;
static uint8_t uart_rx_byte;
static char    rx_buf[UART_BUF_SIZE];
static uint32_t rx_len;

/* ------------------------------------------------------------------ */
/* Internal: send AT command, collect response until OK/ERROR/timeout */
/* ------------------------------------------------------------------ */
static int at_send(const char *cmd, char *resp, uint32_t resp_size,
                   uint32_t timeout_ms) {
    if (!huart) return -1;

    /* Flush pending RX bytes */
    uint8_t discard;
    while (HAL_UART_Receive(huart, &discard, 1, 5) == HAL_OK) {}

    /* Send command + CRLF */
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    HAL_UART_Transmit(huart, (uint8_t *)buf, (uint16_t)n, 1000);

    /* Collect response byte-by-byte until terminator or timeout */
    memset(resp, 0, resp_size);
    uint32_t idx   = 0;
    uint32_t start = HAL_GetTick();

    while (HAL_GetTick() - start < timeout_ms && idx < resp_size - 1) {
        uint8_t b;
        if (HAL_UART_Receive(huart, &b, 1, 20) == HAL_OK) {
            resp[idx++] = (char)b;
            /* Check for final response line */
            if (strstr(resp, "\nOK\r") || strstr(resp, "\nERROR\r")) break;
        }
    }

    LOG_D(TAG, "AT> %s | RSP: %s", cmd, resp);
    return strstr(resp, "ERROR") ? -1 : 0;
}

/* ------------------------------------------------------------------ */
void modem_init(UART_HandleTypeDef *h) {
    huart = h;
}

/* ------------------------------------------------------------------ */
int modem_power_on(void) {
    /* Already on? */
    if (HAL_GPIO_ReadPin(BC65_STATUS_PORT, BC65_STATUS_PIN) == GPIO_PIN_SET)
        return 0;

    /* Pulse PWRKEY LOW for PWRKEY_PULSE_MS ms */
    HAL_GPIO_WritePin(BC65_PWRKEY_PORT, BC65_PWRKEY_PIN, GPIO_PIN_RESET);
    HAL_Delay(PWRKEY_PULSE_MS);
    HAL_GPIO_WritePin(BC65_PWRKEY_PORT, BC65_PWRKEY_PIN, GPIO_PIN_SET);

    /* Wait for STATUS to go HIGH */
    uint32_t t = HAL_GetTick();
    while (HAL_GetTick() - t < POWER_ON_TIMEOUT_MS) {
        if (HAL_GPIO_ReadPin(BC65_STATUS_PORT, BC65_STATUS_PIN) == GPIO_PIN_SET) {
            LOG_I(TAG, "BC65 powered on");
            HAL_Delay(500);     /* brief settle before AT commands */
            /* Disable echo for clean parsing */
            char resp[64];
            at_send("ATE0", resp, sizeof(resp), 1000);
            return 0;
        }
        HAL_Delay(100);
    }

    LOG_E(TAG, "BC65 power-on timeout");
    return -1;
}

/* ------------------------------------------------------------------ */
int modem_power_off(void) {
    if (HAL_GPIO_ReadPin(BC65_STATUS_PORT, BC65_STATUS_PIN) == GPIO_PIN_RESET)
        return 0;   /* already off */

    HAL_GPIO_WritePin(BC65_PWRKEY_PORT, BC65_PWRKEY_PIN, GPIO_PIN_RESET);
    HAL_Delay(PWRKEY_PULSE_MS);
    HAL_GPIO_WritePin(BC65_PWRKEY_PORT, BC65_PWRKEY_PIN, GPIO_PIN_SET);

    uint32_t t = HAL_GetTick();
    while (HAL_GetTick() - t < 5000) {
        if (HAL_GPIO_ReadPin(BC65_STATUS_PORT, BC65_STATUS_PIN) == GPIO_PIN_RESET) {
            LOG_I(TAG, "BC65 powered off");
            return 0;
        }
        HAL_Delay(100);
    }
    return -1;
}

/* ------------------------------------------------------------------ */
int modem_check_registration(void) {
    char resp[256];
    if (at_send("AT+CEREG?", resp, sizeof(resp), 2000) != 0) return -1;

    int stat = -1;
    sscanf(resp, "+CEREG: %*d,%d", &stat);
    return stat;
}

/* ------------------------------------------------------------------ */
int modem_read_serving_cell(CellData_t *cell_data) {
    char resp[512];
    if (at_send("AT+QENG=\"servingcell\"", resp, sizeof(resp), 5000) != 0)
        return -1;

    /*
     * Response (LTE FDD example):
     * +QENG: "servingcell","FDD",214,03,6936965,235,3050,7,5,5,8CA,18,-98,-11,-72,7,59,5
     * Tok:   [0]            [1]  [2] [3][4]       [5][6] ...    [10] [11][12]
     *                       RAT  MCC MNC CID                    TAC       RSRP
     *
     * CRITICAL: CID and TAC are hexadecimal strings — use strtoul base 16.
     */
    char buf[512];
    strncpy(buf, resp, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok[25];
    int ntok = 0;
    char *p = strtok(buf, ",");
    while (p && ntok < 25) {
        while (*p == ' ' || *p == '"') p++;
        char *end = p + strlen(p) - 1;
        while (end > p && (*end == ' ' || *end == '"' || *end == '\r' || *end == '\n'))
            *end-- = '\0';
        tok[ntok++] = p;
        p = strtok(NULL, ",");
    }

    if (ntok < 13) {
        LOG_E(TAG, "QENG: too few tokens (%d)", ntok);
        return -1;
    }

    cell_data->mcc     = (uint16_t)atoi(tok[2]);
    cell_data->mnc     = (uint16_t)atoi(tok[3]);
    cell_data->cell_id = (uint32_t)strtoul(tok[4],  NULL, 16);
    cell_data->tac     = (uint16_t)strtoul(tok[10], NULL, 16);
    cell_data->rsrp    = (int16_t)atoi(tok[12]);

    LOG_I(TAG, "Cell MCC=%d MNC=%d CID=0x%lX TAC=0x%X RSRP=%d",
          cell_data->mcc, cell_data->mnc,
          (unsigned long)cell_data->cell_id, cell_data->tac,
          (int)cell_data->rsrp);
    return 0;
}

/* ------------------------------------------------------------------ */
/* UTC date+time → Unix timestamp (no mktime, no timezone dependency)  */
/* ------------------------------------------------------------------ */
static uint32_t utc_to_unix(int year, int mon, int day, int h, int m, int s) {
    static const uint8_t dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t days = 0;
    for (int y = 1970; y < year; y++)
        days += ((y % 4 == 0) && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    int leap = ((year % 4 == 0) && (year % 100 != 0 || year % 400 == 0));
    for (int mo = 1; mo < mon; mo++) {
        days += dim[mo - 1];
        if (mo == 2 && leap) days++;
    }
    days += day - 1;
    return days * 86400U + (uint32_t)h * 3600 + (uint32_t)m * 60 + (uint32_t)s;
}

/* ------------------------------------------------------------------ */
int modem_get_unix_time(uint32_t *ts_out) {
    *ts_out = 0;
    char resp[128];

    /* 1. Network time via LTE SIB16 (available in LIMSRV mode, no SIM needed) */
    int ok = at_send("AT+QLTS=1", resp, sizeof(resp), 2000);
    if (ok != 0)
        /* 2. Fallback to internal RTC */
        ok = at_send("AT+CCLK?", resp, sizeof(resp), 1000);
    if (ok != 0) return -1;

    char *q = strchr(resp, '"');
    if (!q) return -1;
    q++;

    int yy, mo, dd, hh, mm, ss, tz_qh = 0;
    char tz_sign = '+';
    if (sscanf(q, "%d/%d/%d,%d:%d:%d%c%d",
               &yy, &mo, &dd, &hh, &mm, &ss, &tz_sign, &tz_qh) < 6)
        return -1;

    int year = 2000 + yy;
    if (year < 2025) return 0;  /* RTC not yet synced */

    uint32_t local = utc_to_unix(year, mo, dd, hh, mm, ss);
    int tz_sec = tz_qh * 15 * 60;
    *ts_out = (tz_sign == '+') ? local - (uint32_t)tz_sec
                               : local + (uint32_t)tz_sec;

    LOG_I(TAG, "Time: %04d-%02d-%02d %02d:%02d:%02d → Unix %lu",
          year, mo, dd, hh, mm, ss, (unsigned long)*ts_out);
    return 0;
}

/* ------------------------------------------------------------------ */
int modem_enter_psm(uint32_t sleep_seconds) {
    char cmd[64], resp[128];
    /* T3412 TAU timer: units of 10 min → ceil(sleep_seconds / 600) */
    int tau_units = (int)(sleep_seconds / 600) + 1;
    snprintf(cmd, sizeof(cmd), "AT+QPSMS=1,,,%d", tau_units);
    if (at_send(cmd, resp, sizeof(resp), 2000) != 0) {
        LOG_E(TAG, "PSM enable failed");
        return -1;
    }
    LOG_I(TAG, "BC65 PSM armed (%lu s)", (unsigned long)sleep_seconds);
    return 0;
}

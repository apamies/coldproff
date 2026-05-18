/* modem.h — BC65 NB-IoT modem driver (STM32 HAL, AT commands via USART2)
 *
 * BC65 is a pure AT-command modem (no QuecOpen).
 * This driver runs on the STM32L010F4P6 host MCU.
 *
 * Pin assignments (PA port):
 *   PA0 — BC65_STATUS  (input,  HIGH = module powered on)
 *   PA1 — BC65_PWRKEY  (output, active-LOW pulse ≥650 ms to toggle power)
 *   PA2 — USART2_TX    (to BC65 UART_RX)
 *   PA3 — USART2_RX    (from BC65 UART_TX)
 *   PA4 — BC65_RESET_N (output, active-LOW, optional)
 */
#ifndef MODEM_H
#define MODEM_H

#include "stm32l0xx_hal.h"
#include <stdint.h>

/* Shared data structure used by all modules */
typedef struct {
    uint32_t timestamp;       /* Unix UTC (0 = unavailable — server extrapolates) */
    uint32_t cell_id;         /* Cell ID, hex-parsed from AT+QENG */
    uint16_t mcc;
    uint16_t mnc;
    uint16_t tac;             /* TAC, hex-parsed */
    int16_t  rsrp;            /* RSRP in dBm */
    int16_t  temp_raw;        /* TMP117 raw ADC value (7.8125 m°C / LSB) */
    uint8_t  measurement_count;
} CellData_t;

/* GPIO defines — must match MX_GPIO_Init() in main.c */
#define BC65_STATUS_PIN    GPIO_PIN_0
#define BC65_STATUS_PORT   GPIOA
#define BC65_PWRKEY_PIN    GPIO_PIN_1
#define BC65_PWRKEY_PORT   GPIOA
#define BC65_RESET_PIN     GPIO_PIN_4
#define BC65_RESET_PORT    GPIOA

/* Pass the HAL UART handle created by CubeMX */
void modem_init(UART_HandleTypeDef *huart);

/* Toggle PWRKEY to power on; waits for STATUS=HIGH. Returns 0 on success. */
int  modem_power_on(void);

/* Toggle PWRKEY to power off; waits for STATUS=LOW. Returns 0 on success. */
int  modem_power_off(void);

/* Check NB-IoT registration status.
 * Returns: 1=home, 5=roaming, 8=LIMSRV (no SIM), <0=error. */
int  modem_check_registration(void);

/* Read serving cell via AT+QENG="servingcell". Fills cell_data. */
int  modem_read_serving_cell(CellData_t *cell_data);

/* Get UTC Unix timestamp from BC65 RTC (AT+QLTS=1 → AT+CCLK?).
 * Sets *ts_out = 0 if unavailable (year < 2025). */
int  modem_get_unix_time(uint32_t *ts_out);

/* Enable PSM: BC65 will enter ultra-low-power sleep.
 * STM32 must independently enter STOP mode for the same duration. */
int  modem_enter_psm(uint32_t sleep_seconds);

#endif /* MODEM_H */

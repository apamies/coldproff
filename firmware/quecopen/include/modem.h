#ifndef __MODEM_H__
#define __MODEM_H__

#include <stdint.h>

typedef struct {
    uint32_t timestamp;        // Unix UTC seconds (0 = unavailable)
    uint32_t cell_id;          // Parsed from hex (AT+QENG)
    uint16_t mcc, mnc, tac;
    int16_t  rsrp;             // dBm
    int16_t  temp_raw;         // TMP117 raw (LSB = 7.8125 m°C)
    uint8_t  measurement_count;
} CellData_t;

int modem_init(void);
int modem_check_registration(void);
int modem_read_serving_cell(CellData_t *cell_data);
int modem_get_unix_time(uint32_t *ts_out);   // AT+QLTS=1, fallback AT+CCLK?
int modem_enter_psm(uint32_t sleep_seconds);
int modem_exit_psm(void);
void modem_close(void);

#endif  // __MODEM_H__

#ifndef __MODEM_H__
#define __MODEM_H__

#include <stdint.h>

typedef struct {
    uint16_t mcc, mnc, tac;
    uint32_t cell_id;
    int16_t rsrp;
    int16_t temp_raw;
    uint8_t measurement_count;
} CellData_t;

int modem_init(void);
int modem_check_registration(void);
int modem_read_serving_cell(CellData_t *cell_data);
int modem_enter_psm(uint32_t sleep_seconds);
int modem_exit_psm(void);
void modem_close(void);

#endif  // __MODEM_H__

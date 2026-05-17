/*
 * ColdProff - Main firmware entry point
 * BC660K-GL NB-IoT + TMP117 + ST25DV64K
 * Quectel QuecOpen (FreeRTOS)
 */

#include "ql_api_osi.h"
#include "ql_api_common.h"
#include "ql_log.h"
#include "modem.h"
#include "sensor.h"
#include "nfc.h"
#include "geoloc.h"

#define TAG "COLDPROFF"

// Task handles
ql_task_t main_task_ref = NULL;

// Global state
typedef struct {
    uint16_t mcc, mnc, tac;
    uint32_t cell_id;
    int16_t rsrp;
    int16_t temp_raw;  // TMP117 raw value
    uint8_t measurement_count;
} CellData_t;

static CellData_t cell_data = {0};

/*
 * Measurement cycle:
 * 1. Read AT+QENG (cell info)
 * 2. Read TMP117 (temperature)
 * 3. Write to ST25DV64K via I2C
 * 4. Sleep 3h in PSM (800nA)
 * 5. Repeat 56 times (~7 days)
 */
static void measurement_task(void *argv) {
    QL_LOG_INFO(TAG, "Starting measurement cycle...");

    int cycle = 0;
    const int max_cycles = 56;  // 7 days @ 3h interval

    while (cycle < max_cycles) {
        QL_LOG_INFO(TAG, "[%d/56] Reading cell + sensor", cycle + 1);

        // Step 1: Get cell info via AT+QENG
        if (modem_read_serving_cell(&cell_data) != 0) {
            QL_LOG_ERR(TAG, "Failed to read serving cell");
            goto sleep;
        }

        QL_LOG_INFO(TAG, "Cell: MCC=%d MNC=%d LAC=0x%X CID=0x%X RSRP=%d",
                   cell_data.mcc, cell_data.mnc,
                   cell_data.tac, cell_data.cell_id,
                   cell_data.rsrp);

        // Step 2: Read temperature sensor
        if (sensor_read_temp(&cell_data.temp_raw) != 0) {
            QL_LOG_ERR(TAG, "Failed to read temperature");
            goto sleep;
        }

        QL_LOG_INFO(TAG, "Temp: %d (raw)", cell_data.temp_raw);

        // Step 3: Write to NFC EEPROM
        if (nfc_write_record(&cell_data) != 0) {
            QL_LOG_ERR(TAG, "Failed to write NFC record");
        }

        cell_data.measurement_count++;

sleep:
        QL_LOG_INFO(TAG, "Entering PSM sleep for 3h...");

        // Enable PSM: 3 hours = 10800 seconds
        modem_enter_psm(10800);

        cycle++;

        // In real device: modem wakes automatically after PSM timer
        // For testing: simulate with delay
        ql_rtos_task_sleep_s(10);
    }

    QL_LOG_INFO(TAG, "Measurement cycle complete (7 days)");
    QL_LOG_INFO(TAG, "Total readings: %d", cell_data.measurement_count);

    // Device can be powered off or enter deep sleep
    while (1) {
        ql_rtos_task_sleep_s(600);  // Idle for 10min
    }
}

void ql_main(void) {
    QL_LOG_INFO(TAG, "=== ColdProff FW v0.1 ===");

    // Initialize I2C (for TMP117 + ST25DV64K)
    if (sensor_i2c_init() != 0) {
        QL_LOG_ERR(TAG, "I2C initialization failed");
        return;
    }

    if (nfc_i2c_init() != 0) {
        QL_LOG_ERR(TAG, "NFC I2C initialization failed");
        return;
    }

    // Initialize modem (AT commands)
    if (modem_init() != 0) {
        QL_LOG_ERR(TAG, "Modem initialization failed");
        return;
    }

    // Check if device registered without SIM (LIMSRV mode)
    int reg_status = modem_check_registration();
    if (reg_status < 0) {
        QL_LOG_ERR(TAG, "Modem not registered");
        return;
    }

    QL_LOG_INFO(TAG, "Registration status: %d (1=home, 5=roaming, 8=LIMSRV)", reg_status);

    // Create main measurement task
    ql_os_task_create(&main_task_ref, 4096, 20, "measurement", measurement_task, NULL);

    if (main_task_ref == NULL) {
        QL_LOG_ERR(TAG, "Failed to create measurement task");
        return;
    }
}

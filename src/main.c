/******************************************************************************
 * FILENAME : main.c
 *
 * DESCRIPTION :
 *
 * PUBLIC LICENSE :
 * This code is public and free to modify under the terms of the
 * GNU General Public License (GPL v3) or later. It is provided "as is",
 * without warranties of any kind.
 *
 * AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
 ******************************************************************************/

// Standard headers
#include <assert.h>
#include <stdio.h>

// FreeRTOS headers
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// ESP-IDF headers
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>

// Project headers
#include "config.h"
#include "data_structures.h"
#include "system.h"

static const char *TAG = "STF_P1:main";

// Entry point
void app_main(void) {
    // Create a system instance and register states
    system_t sys_stf_p1;
    ESP_LOGI(TAG, "Starting STF_P1 system");
    system_create(&sys_stf_p1, SYS_NAME);
    system_register_state(&sys_stf_p1, INIT);
    system_register_state(&sys_stf_p1, SENSOR_LOOP);
    system_register_state(&sys_stf_p1, NORMAL_MODE);
    system_register_state(&sys_stf_p1, DEGRADED_MODE);
    system_register_state(&sys_stf_p1, ERROR);
    system_set_default_state(&sys_stf_p1, INIT);

    // Define task handles
    system_task_t task_sensor;
    system_task_t task_checker;
    system_task_t task_monitor;

    // Create ring buffers for inter-task communication
    // Ring buffer between Sensor/Monitor and Checker/Monitor
    RingbufHandle_t monitor_buf = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);
    // Ring buffer between Sensor and Checker tasks
    RingbufHandle_t checker_buf = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);

    // Check if ring buffers were created successfully
    if (monitor_buf == NULL || checker_buf == NULL) {
        ESP_LOGE(TAG, "Failed to create ring buffers");
        return;
    }

    // Variable for return codes
    esp_err_t ret;

    // State machine
    STATE_MACHINE(sys_stf_p1) {
        STATE_MACHINE_BEGIN();

        STATE(INIT) {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: INIT");

            // Initialize NVS (if needed)
            ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ESP_ERROR_CHECK(nvs_flash_init());
            }

            // Start Sensor task
            ESP_LOGI(TAG, "Starting Sensor task...");
             task_sensor_args_t task_sensor_args = {
                .monitor_buf = &monitor_buf,
                .checker_buf = &checker_buf,
                .freq = SENSOR_FREQUENCY,         // Define SENSOR_FREQUENCY in config.h
                .checker_period = CHECKER_PERIOD  // Define CHECKER_PERIOD in config.h
            };
            system_task_start_in_core(&sys_stf_p1, &task_sensor, TASK_SENSOR, "TASK_SENSOR",
                                      TASK_SENSOR_STACK_SIZE, &task_sensor_args, 0, CORE0);
            ESP_LOGI(TAG, "Sensor task started");

            // Start Checker task
            ESP_LOGI(TAG, "Starting Checker task...");
            task_checker_args_t task_checker_args = {
                .checker_buf = &checker_buf,
                .monitor_buf = &monitor_buf};
            system_task_start_in_core(&sys_stf_p1, &task_checker, TASK_CHECKER, "TASK_CHECKER",
                                      TASK_CHECKER_STACK_SIZE, &task_checker_args, 0, CORE0);
            ESP_LOGI(TAG, "Checker task started");

            // Start Monitor task
            ESP_LOGI(TAG, "Starting Monitor task...");
            task_monitor_args_t task_monitor_args = {
                .monitor_buf = &monitor_buf};
            system_task_start_in_core(&sys_stf_p1, &task_monitor, TASK_MONITOR, "TASK_MONITOR",
                                      TASK_MONITOR_STACK_SIZE, &task_monitor_args, 0, CORE1);
            ESP_LOGI(TAG, "Monitor task started");

            // Delay to ensure tasks start properly
            vTaskDelay(pdMS_TO_TICKS(1000));

            // Transition to SENSOR_LOOP state
            SWITCH_ST(&sys_stf_p1, SENSOR_LOOP);
            STATE_END();
        }

        STATE(SENSOR_LOOP) {
            STATE_BEGIN();
            // The system remains in this state indefinitely
            ESP_LOGI(TAG, "State: SENSOR_LOOP");
            STATE_END();
        }
        
        STATE(NORMAL_MODE) {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: NORMAL_MODE");
            // Handle normal mode operations
            STATE_END();
        }

        STATE(DEGRADED_MODE) {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: DEGRADED_MODE");
            // Handle degraded mode operations
            STATE_END();
        }

        STATE(ERROR) {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: ERROR");

            // Stop Sensor task
            ESP_LOGI(TAG, "Stopping Sensor task...");
            system_task_stop(&sys_stf_p1, &task_sensor, TASK_SENSOR_TIMEOUT_MS);

            // Stop Checker task
            ESP_LOGI(TAG, "Stopping Checker task...");
            system_task_stop(&sys_stf_p1, &task_checker, TASK_CHECKER_TIMEOUT_MS);

            // Handle error state operations
            STATE_END();
        }

        STATE_MACHINE_END();
    }
}
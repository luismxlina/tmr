// task_monitor.c

#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

// FreeRTOS
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>

// ESP
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>

// Project includes
#include "config.h"
#include "data_structures.h"

static const char *TAG = "STF_P1:task_monitor";

// Monitor Task
SYSTEM_TASK(TASK_MONITOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Monitor running");

    // Retrieve task arguments
    task_monitor_args_t *ptr_args = (task_monitor_args_t *)TASK_ARGS;
    RingbufHandle_t *monitor_buf = ptr_args->monitor_buf;

    // Variables
    size_t item_size;
    sensor_data_t *received_data;

    // Loop
    TASK_LOOP() {
        // Wait to receive data from the ring buffer
        received_data = (sensor_data_t *)xRingbufferReceive(*monitor_buf, &item_size, portMAX_DELAY);

        if (received_data != NULL && item_size == sizeof(sensor_data_t)) {
            // Check the source of the data
            if (received_data->source == DATA_SOURCE_SENSOR) {
                // Data from Sensor task
                ESP_LOGI(TAG, "Sensor Data: T1 = %.2f°C", received_data->temperature1);
            } else if (received_data->source == DATA_SOURCE_CHECKER) {
                // Data from Checker task
                ESP_LOGI(TAG, "Checker Data: Deviation = %.2f°C between T1 = %.2f°C and T2 = %.2f°C",
                         received_data->deviation, received_data->temperature1, received_data->temperature2);
            } else {
                // Unknown source
                ESP_LOGW(TAG, "Unknown data source");
            }

            // Return the item to the ring buffer
            vRingbufferReturnItem(*monitor_buf, (void *)received_data);
        } else {
            ESP_LOGW(TAG, "No data received or incorrect size");
        }
    }

    ESP_LOGI(TAG, "Stopping Monitor task...");
    TASK_END();
}
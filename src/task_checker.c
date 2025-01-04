// task_checker.c

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "data_structures.h"
#include "system.h"

static const char* TAG = "STF_P1:task_checker";

// Checker Task
SYSTEM_TASK(TASK_CHECKER) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Checker running");

    // Retrieve the task arguments
    task_checker_args_t* ptr_args = (task_checker_args_t*)TASK_ARGS;
    RingbufHandle_t* checker_buf = ptr_args->checker_buf;  // Input RingBuffer from Sensor task
    RingbufHandle_t* monitor_buf = ptr_args->monitor_buf;  // Output RingBuffer to Monitor task

    // Variables
    sensor_data_t* received_data;  // Pointer to data received from Sensor
    sensor_data_t checker_data;    // Data to send to Monitor
    size_t item_size;

    // Loop
    TASK_LOOP() {
        // Wait to receive data from the Checker RingBuffer
        received_data = (sensor_data_t*)xRingbufferReceive(*checker_buf, &item_size, pdMS_TO_TICKS(2000));
        if (received_data != NULL && item_size == sizeof(sensor_data_t)) {
            // Calculate deviation
            float deviation = fabsf(received_data->temperature1 - received_data->temperature2);
            ESP_LOGI(TAG, "Calculated deviation: %.2f (T1: %.2f, T2: %.2f)",
                     deviation, received_data->temperature1, received_data->temperature2);

            // Prepare data to send to Monitor
            checker_data.source = DATA_SOURCE_CHECKER;
            checker_data.temperature1 = received_data->temperature1;
            checker_data.temperature2 = received_data->temperature2;
            checker_data.deviation = deviation;

            // Send the data to the Monitor RingBuffer
            if (xRingbufferSend(*monitor_buf, &checker_data, sizeof(sensor_data_t), pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Monitor buffer full (Checker)");
            }

            // Return the item to the Checker RingBuffer
            vRingbufferReturnItem(*checker_buf, (void*)received_data);
        } else {
            ESP_LOGW(TAG, "No data received in Checker or incorrect size");
        }
    }

    ESP_LOGI(TAG, "Stopping Checker task...");
    TASK_END();
}
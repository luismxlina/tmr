// task_sensor.c

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "data_structures.h"
#include "therm.h"

static const char *TAG = "STF_P1:task_sensor";

// Semaphore for timer expiration
static SemaphoreHandle_t semSample = NULL;

// Timer callback function
static void tmrSampleCallback(void *arg) {
    xSemaphoreGive(semSample);
}

// Sensor Task
SYSTEM_TASK(TASK_SENSOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Sensor running");

    // Retrieve task arguments
    task_sensor_args_t *ptr_args = (task_sensor_args_t *)TASK_ARGS;
    RingbufHandle_t *monitor_buf = ptr_args->monitor_buf;
    RingbufHandle_t *checker_buf = ptr_args->checker_buf;
    uint8_t frequency = ptr_args->freq;
    uint64_t period_us = 1000000 / frequency;
    uint8_t N = ptr_args->checker_period;

    // Thermistor configuration
    therm_t t1;
    ESP_ERROR_CHECK(therm_init(&t1, ADC_CHANNEL_6, THERM1_POWER_GPIO,
                               SERIES_RESISTANCE, NOMINAL_RESISTANCE,
                               NOMINAL_TEMPERATURE, BETA_COEFFICIENT));
    therm_t t2;
    ESP_ERROR_CHECK(therm_init(&t2, ADC_CHANNEL_7, THERM2_POWER_GPIO,
                               SERIES_RESISTANCE, NOMINAL_RESISTANCE,
                               NOMINAL_TEMPERATURE, BETA_COEFFICIENT));

    // Initialize semaphore
    semSample = xSemaphoreCreateBinary();
    if (semSample == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        TASK_END();
        return;
    }

    // Timer configuration
    const esp_timer_create_args_t tmrSampleArgs = {
        .callback = &tmrSampleCallback,
        .name = "Sample Timer"};

    // Create and start the timer
    esp_timer_handle_t tmrSample;
    ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));

    // Variables
    uint32_t i = 0;
    float temperature1, temperature2;
    size_t data_size = sizeof(sensor_data_t);

    // Power on T1 once at the beginning
    therm_power_on(t1);
    ESP_LOGI(TAG, "T1 powered on");

    // Loop
    TASK_LOOP() {
        // Wait for semaphore with a calculated timeout
        TickType_t timeout_ticks = ((1000 / frequency) * 1.2) / portTICK_PERIOD_MS;
        if (xSemaphoreTake(semSample, timeout_ticks)) {
            // Read T1 temperature (T1 is already powered on)
            temperature1 = therm_read_temperature(t1);
            ESP_LOGD(TAG, "Read T1: %.2f°C", temperature1);

            // Prepare data for Monitor task
            sensor_data_t monitor_data = {
                .source = DATA_SOURCE_SENSOR,
                .temperature1 = temperature1,
                .temperature2 = 0.0f,
                .deviation = 0.0f};

            // Send to Monitor task
            if (xRingbufferSend(*monitor_buf, &monitor_data, data_size, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Monitor buffer full");
            } else {
                ESP_LOGD(TAG, "Sent data to Monitor: T1 = %.2f°C", temperature1);
            }

            i++;

            // Every N periods, read T2 and send data to Checker task
            if (i % N == 0) {
                // Power on T2
                therm_power_on(t2);
                ESP_LOGD(TAG, "T2 powered on");

                // Read T2 temperature
                temperature2 = therm_read_temperature(t2);
                ESP_LOGD(TAG, "Read T2: %.2f°C", temperature2);

                // Power off T2 after reading
                therm_power_off(t2);
                ESP_LOGD(TAG, "T2 powered off");

                // Prepare data for Checker task
                sensor_data_t checker_data = {
                    .source = DATA_SOURCE_SENSOR,  // Indicates data from Sensor task
                    .temperature1 = temperature1,
                    .temperature2 = temperature2,
                    .deviation = 0.0f  // To be calculated by Checker task
                };

                // Send to Checker task
                if (xRingbufferSend(*checker_buf, &checker_data, data_size, pdMS_TO_TICKS(100)) != pdTRUE) {
                    ESP_LOGW(TAG, "Checker buffer full");
                } else {
                    ESP_LOGD(TAG, "Sent data to Checker: T1 = %.2f°C, T2 = %.2f°C", temperature1, temperature2);
                }
            }
        } else {
            ESP_LOGI(TAG, "Watchdog (soft) failed");
            esp_restart();
        }
    }

    ESP_LOGI(TAG, "Stopping Sensor task...");
    // Clean up
    ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
    ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
    therm_power_off(t1);  // Ensure T1 is powered off when task ends
    TASK_END();
}
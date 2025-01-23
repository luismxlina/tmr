#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"

static const char* TAG = "STF_P1:task_votador";

// Tarea VOTADOR
SYSTEM_TASK(TASK_VOTER) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Votador running");

    // Recibe los argumentos de configuración de la tarea y los desempaqueta
    task_voter_args_t* ptr_args = (task_voter_args_t*)TASK_ARGS;
    RingbufHandle_t* rbuf_sensor = ptr_args->rbuf_sensor;
    RingbufHandle_t* rbuf_monitor = ptr_args->rbuf_monitor;

    // Variables para reutilizar en el bucle
    size_t length;
    void* ptr;
    float temperature1, temperature2, temperature3;
    float average_temperature;

    // Loop
    TASK_LOOP() {
        // Se bloquea en espera de que haya algo que leer en RingBuffer.
        ptr = xRingbufferReceive(*rbuf_sensor, &length, pdMS_TO_TICKS(portMAX_DELAY));

        // Si el timeout expira, este puntero es NULL
        if (ptr != NULL) {
            // Lee los valores de los tres termistores
            float* data_ptr = (float*)ptr;
            temperature1 = data_ptr[0];
            temperature2 = data_ptr[1];
            temperature3 = data_ptr[2];
            // Calcula el promedio
            average_temperature = (temperature1 + temperature2 + temperature3) / 3;

            // Devuelve el item al RingBuffer
            vRingbufferReturnItem(*rbuf_sensor, ptr);

            // Enviar el promedio al RingBuffer de la tarea monitor
            if (xRingbufferSendAcquire(*rbuf_monitor, &ptr, sizeof(float), pdMS_TO_TICKS(100)) == pdTRUE) {
                float* avg_ptr = (float*)ptr;
                *avg_ptr = average_temperature;
                xRingbufferSendComplete(*rbuf_monitor, ptr);
            } else {
                ESP_LOGI(TAG, "Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*rbuf_monitor));
            }
        } else {
            ESP_LOGW(TAG, "Esperando datos ...");
        }
    }

    ESP_LOGI(TAG, "Deteniendo la tarea...");
    TASK_END();
}
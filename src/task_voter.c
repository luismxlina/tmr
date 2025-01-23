#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
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
    uint16_t mask = ptr_args->mask;

    // Variables para reutilizar en el bucle
    size_t length;
    void* ptr;
    uint16_t lsb1, lsb2, lsb3;
    uint16_t result;

    // Loop
    TASK_LOOP() {
        // Se bloquea en espera de que haya algo que leer en RingBuffer.
        ptr = xRingbufferReceive(*rbuf_sensor, &length, pdMS_TO_TICKS(portMAX_DELAY));

        // Si el timeout expira, este puntero es NULL
        if (ptr != NULL) {
            // Lee los valores de los tres termistores
            uint16_t* data_ptr = (uint16_t*)ptr;
            lsb1 = data_ptr[0];
            lsb2 = data_ptr[1];
            lsb3 = data_ptr[2];

            // Aplica la máscara binaria
            lsb1 &= mask;
            lsb2 &= mask;
            lsb3 &= mask;

            // Calcula el valor resultante usando operadores lógicos booleanos
            result = (lsb1 & lsb2) | (lsb1 & lsb3) | (lsb2 & lsb3);

            // ESP_LOGI(TAG, "Masked values: %u, %u, %u", lsb1, lsb2, lsb3);
            // ESP_LOGI(TAG, "Result: %u", result);

            // Devuelve el item al RingBuffer
            vRingbufferReturnItem(*rbuf_sensor, ptr);

            // Enviar el resultado al RingBuffer de la tarea monitor
            if (xRingbufferSendAcquire(*rbuf_monitor, &ptr, sizeof(uint16_t), pdMS_TO_TICKS(100)) == pdTRUE) {
                uint16_t* result_ptr = (uint16_t*)ptr;
                *result_ptr = result;
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
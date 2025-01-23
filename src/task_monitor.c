/**********************************************************************
 * FILENAME : task_monitor.c
 *
 * DESCRIPTION :
 *
 * PUBLIC LICENSE :
 * Este código es de uso público y libre de modificar bajo los términos de la
 * Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
 * sin garantías de ningún tipo.
 *
 * AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
 ******************************************************************************/

// libc
#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>

// esp
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>

// propias
#include "config.h"
#include "therm.h"

static const char *TAG = "STF_P1:task_monitor";

// Tarea MONITOR
SYSTEM_TASK(TASK_MONITOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Monitor running");

    // Recibe los argumentos de configuración de la tarea y los desempaqueta
    task_monitor_args_t *ptr_args = (task_monitor_args_t *)TASK_ARGS;
    RingbufHandle_t *rbuf_monitor = ptr_args->rbuf_monitor;

    // Variable para reutilizar en el bucle
    size_t length;
    void *ptr;
    uint16_t result;
    float temperature;

    // Loop
    TASK_LOOP() {
        // Se bloquea en espera de que haya algo que leer en RingBuffer.
        // Tiene un timeout de 1 segundo para no bloquear indefinidamente la tarea,
        // pero si expira vuelve aquí sin consecuencias
        ptr = xRingbufferReceive(*rbuf_monitor, &length, pdMS_TO_TICKS(1000));

        // Si el timeout expira, este puntero es NULL
        if (ptr != NULL) {
            // Lee el valor resultante del votador
            result = *((uint16_t *)ptr);

            // Convierte el valor resultante a temperatura en ºC
            temperature = therm_calculate_temperature(result);

            // Muestra el valor promedio
            ESP_LOGI(TAG, "Average Temperature: %.5fºC", temperature);

            // Devuelve el item al RingBuffer
            vRingbufferReturnItem(*rbuf_monitor, ptr);
        } else {
            ESP_LOGW(TAG, "Esperando datos ...");
        }
    }
    ESP_LOGI(TAG, "Deteniendo la tarea ...");
    TASK_END();
}
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
#include "therm.h"

static const char* TAG = "STF_P1:task_sensor";

// Esta tarea temporiza la lectura de un termistor.
// Para implementar el periodo de muestreo, se utiliza un semáforo declarado de forma global
// para poder ser utilizado desde esta rutina de expiración del timer.
// Cada vez que el temporizador expira, libera el semáforo
// para que la tarea realice una iteración.
static SemaphoreHandle_t semSample = NULL;
static void tmrSampleCallback(void* arg) {
    xSemaphoreGive(semSample);
}

// Tarea SENSOR
SYSTEM_TASK(TASK_SENSOR) {
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Sensor running");

    // Recibe los argumentos de configuración de la tarea y los desempaqueta
    task_sensor_args_t* ptr_args = (task_sensor_args_t*)TASK_ARGS;
    RingbufHandle_t* rbuf_sensor = ptr_args->rbuf_sensor;
    uint8_t frequency = ptr_args->freq;
    uint64_t period_us = 1000000 / frequency;

    // Configuración de los termistores
    therm_t t1, t2, t3;
    ESP_ERROR_CHECK(therm_init(&t1, ADC_CHANNEL_5, THERM1_POWER_GPIO,
                               SERIES_RESISTANCE, NOMINAL_RESISTANCE,
                               NOMINAL_TEMPERATURE, BETA_COEFFICIENT));
    ESP_ERROR_CHECK(therm_init(&t2, ADC_CHANNEL_6, THERM2_POWER_GPIO,
                               SERIES_RESISTANCE, NOMINAL_RESISTANCE,
                               NOMINAL_TEMPERATURE, BETA_COEFFICIENT));
    ESP_ERROR_CHECK(therm_init(&t3, ADC_CHANNEL_7, THERM3_POWER_GPIO,
                               SERIES_RESISTANCE, NOMINAL_RESISTANCE,
                               NOMINAL_TEMPERATURE, BETA_COEFFICIENT));

    therm_power_on(t1);
    therm_power_on(t2);
    therm_power_on(t3);

    // Inicializa el semáforo (la estructura del manejador se definió globalmente)
    semSample = xSemaphoreCreateBinary();

    // Crea y establece una estructura de configuración para el temporizador
    const esp_timer_create_args_t tmrSampleArgs = {
        .callback = &tmrSampleCallback,
        .name = "Timer Configuration"};

    // Lanza el temporizador, con el periodo de muestreo recibido como parámetro
    esp_timer_handle_t tmrSample;
    ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));

    // Variables para reutilizar en el bucle
    void* ptr;
    float temperature1, temperature2, temperature3;

    // Loop
    TASK_LOOP() {
        // Se bloquea a la espera del semáforo. Si el periodo establecido se retrasa un 20%
        // el sistema se reinicia por seguridad. Este mecanismo de watchdog software es útil
        // en tareas periódicas cuyo periodo es conocido.
        if (xSemaphoreTake(semSample, ((1000 / frequency) * 1.2) / portTICK_PERIOD_MS)) {
            // Lectura del sensor
            temperature1 = therm_read_temperature(t1);
            temperature2 = therm_read_temperature(t2);
            temperature3 = therm_read_temperature(t3);

            // Uso del buffer cíclico entre la tarea sensor y votador. Ver documentación en ESP-IDF
            // Pide al RingBuffer espacio para escribir tres floats.
            if (xRingbufferSendAcquire(*rbuf_sensor, &ptr, 3 * sizeof(float), pdMS_TO_TICKS(100)) != pdTRUE) {
                // Si falla la reserva de memoria, notifica la pérdida del dato. Esto ocurre cuando
                // una tarea productora es mucho más rápida que la tarea consumidora. Aquí no debe ocurrir.
                ESP_LOGI(TAG, "Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*rbuf_sensor));
            } else {
                // Si xRingbufferSendAcquire tiene éxito, podemos escribir el número de bytes solicitados
                // en el puntero ptr. El espacio asignado estará bloqueado para su lectura hasta que
                // se notifique que se ha completado la escritura
                float* data_ptr = (float*)ptr;
                data_ptr[0] = temperature1;
                data_ptr[1] = temperature2;
                data_ptr[2] = temperature3;

                // Se notifica que la escritura ha completado.
                xRingbufferSendComplete(*rbuf_sensor, ptr);
            }
        } else {
            ESP_LOGI(TAG, "Watchdog (soft) failed");
            esp_restart();
        }
    }

    ESP_LOGI(TAG, "Deteniendo la tarea...");
    // Detención controlada de las estructuras que ha levantado la tarea
    ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
    ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
    TASK_END();
}
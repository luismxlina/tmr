/******************************************************************************
 * FILENAME : main.c
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
#include <assert.h>
#include <stdio.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// esp-idf
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>

// propias
#include "config.h"
#include "system.h"

static const char *TAG = "STF_P1:main";

// Punto de entrada
void app_main(void) {
    // Crea una instancia de máquina de estados (ver system.h). Le asigna un nombre
    // registra todos los estados de la máquina, especificando cuál es el estado de partida.
    // Nuestra máquina de estados solo tiene dos; INIT: un estado transitorio de inicialización de
    // los procesos sensor y monitor (un productor y un consumidor); y SENSOR_LOOP: un estado estacionario
    // en el que se queda idefinidamente una vez todo está funcionando.
    system_t sys_stf_p1;
    ESP_LOGI(TAG, "Starting STF_P1 system");
    system_create(&sys_stf_p1, SYS_NAME);
    system_register_state(&sys_stf_p1, INIT);
    system_register_state(&sys_stf_p1, SENSOR_LOOP);
    system_set_default_state(&sys_stf_p1, INIT);

    // Define manejadores de tareas (de momento sin asignar)
    system_task_t task_sensor;
    system_task_t task_monitor;
    system_task_t task_voter;

    // Define y crea buffers cíclicos (ver documentación de ESP-IDF)
    // a modo de buffers thread-safe entre tareas.
    RingbufHandle_t rbuf_sensor;
    RingbufHandle_t rbuf_monitor;
    rbuf_sensor = xRingbufferCreate(BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    rbuf_monitor = xRingbufferCreate(BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);

    // variable para códigos de retorno
    esp_err_t ret;

    // A partir de aquí se establece el código de la máquina de estados,
    // las macros que se utilizan aquí están definidas en system.h/c
    STATE_MACHINE(sys_stf_p1) {
        STATE_MACHINE_BEGIN();
        STATE(INIT) {
            STATE_BEGIN();
            ESP_LOGI(TAG, "State: INIT");

            // Este código se utiliza para inicializar una memoria no volátil del ESP32,
            // es útil cuando queremos almacenar información de nuestro sistema entre
            // apagados, es decir, persistencia.
            // Un ejemplo típico es almacenar en NVS una ESSID y PASS de una red WIFI establecida
            // "en caliente" desde una web mínima que levanta el dispositivo en el primer encendido
            // De momento no lo estamos usando en el proyecto.
            ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ESP_ERROR_CHECK(nvs_flash_init());
            }

            // Crea la tarea sensor como un proceso asociado al CORE 0.
            // Lo que hace la tarea está en task_sensor.h
            ESP_LOGI(TAG, "starting sensor task...");
            task_sensor_args_t task_sensor_args = {&rbuf_sensor, 1};
            system_task_start_in_core(&sys_stf_p1, &task_sensor, TASK_SENSOR, "TASK_SENSOR", TASK_SENSOR_STACK_SIZE, &task_sensor_args, 0, CORE0);
            ESP_LOGI(TAG, "Done");

            // Delay
            vTaskDelay(pdMS_TO_TICKS(1000));

            // Define la máscara binaria
            uint16_t mask = 0xFFF0;  // Máscara binaria de ejemplo

            // Crea la tarea votador como un proceso asociado al CORE 0.
            // Lo que hace la tarea está en task_votador.c
            ESP_LOGI(TAG, "starting voter task...");
            task_voter_args_t task_voter_args = {&rbuf_sensor, &rbuf_monitor, mask};
            system_task_start_in_core(&sys_stf_p1, &task_voter, TASK_VOTER, "TASK_VOTER", TASK_VOTER_STACK_SIZE, &task_voter_args, 0, CORE0);
            ESP_LOGI(TAG, "Done");

            // Delay
            vTaskDelay(pdMS_TO_TICKS(1000));

            // Crea la tarea monitor como un proceso asociado al CORE 1.
            // Lo que hace la tarea está en task_monitor.c
            ESP_LOGI(TAG, "starting monitor task...");
            task_monitor_args_t task_monitor_args = {&rbuf_monitor};
            system_task_start_in_core(&sys_stf_p1, &task_monitor, TASK_MONITOR, "TASK_MONITOR", TASK_MONITOR_STACK_SIZE, &task_monitor_args, 0, CORE1);
            ESP_LOGI(TAG, "Done");

            // Esta macro provoca el cambio de estado a SENSOR_LOOP, en este caso.
            // system.h define una macro para cambiar de estado desde una tarea externa
            // (la tarea actual es main, la tarea principal de FREERTOS). Es decir,
            // desde la tarea sensor o monitor podríamos cambiar el estado de la máquina si,
            // por ejemplo, se detecta un fallo en el proceso.
            SWITCH_ST(&sys_stf_p1, SENSOR_LOOP);
            STATE_END();
        }
        STATE(SENSOR_LOOP) {
            STATE_BEGIN();
            // La máquina queda en este estado de forma indefinida.
            ESP_LOGI(TAG, "State: SENSOR_LOOP");
            STATE_END();
        }
        STATE_MACHINE_END();
    }
}
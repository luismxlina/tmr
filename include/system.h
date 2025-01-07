/***********************************************************************
 * FILENAME : system.h
 *
 * DESCRIPTION :
 *       Abstraction module to create systems that function as state machines. It allows the creation
 *       of tasks that can be stopped in a controlled manner, and can safely change the state of the machine.
 *
 * PUBLIC FUNCTIONS :
 *       system_create
 *       system_register_state
 *       system_set_default_state
 *       system_task_start
 *       system_task_start_in_core
 *		system_task_stop
 *
 * MACROS:
 *		STATE_MACHINE(system)
 *		STATE_MACHINE_BEGIN() /STATE_MACHINE_END()
 *		STATE(state)
 *		STATE_BEGIN()/STATE_END()
 *       SYSTEM_TASK(task)
 *		TASK_BEGIN()/TASK_END()
 *		TASK_ARGS
 *		TASK_LOOP()
 *		SWITCH_ST_FROM_TASK(state)
 *		GET_ST_FROM_TASK()
 *
 * PUBLIC LICENSE :
 * Este código es de uso público y libre de modificar bajo los términos de la
 * Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
 * sin garantías de ningún tipo.
 *
 * AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
 ******************************************************************************/

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// system
typedef struct
{
    char sys_id[16];                          // system id
    SemaphoreHandle_t sys_st_mutex;           // mutex to change the system state
    SemaphoreHandle_t sys_new_state;          // lock to wait a new state
    uint8_t sys_state;                        // system current state
    uint8_t sys_nstates;                      // number of states
    esp_event_loop_handle_t sys_evt_loop;     // system event loop handler
    esp_event_loop_args_t sys_evt_loop_args;  // system event loop configuration
} system_t;

// system tasks
typedef struct
{
    system_t *system;
    SemaphoreHandle_t sys_task_stop;
    TaskHandle_t sys_task_handler;
    void *sys_task_args;
} system_task_t;

/**
 * The function `system_create` creates a system object with a given ID and initializes its mutexes and
 * event loop.
 *
 * @param sys A pointer to a structure of type system_t, which represents the system being created.
 * @param id The id parameter is a string that represents the unique identifier for the system. It is
 * used to name the system event loop task and is also copied into the sys_id field of the system_t
 * structure.
 */
void system_create(system_t *sys, const char *id);

// system add state
/**
 * The function `system_register_state` registers a system state with an event loop and increments the
 * number of states in the system.
 *
 * @param sys A pointer to the system structure that contains information about the system.
 * @param st The parameter "st" is of type uint8_t, which means it is an unsigned 8-bit integer. It is
 * used to represent the state that is being registered in the system.
 */
void system_register_state(system_t *sys, uint8_t st);

// system set default state
/**
 * The function sets the default state of a system and gives a semaphore if it is not already taken.
 *
 * @param sys A pointer to a structure of type system_t, which represents the system.
 * @param default_st The default state to set for the system.
 */
void system_set_default_state(system_t *sys, uint8_t default_st);

// system task start
/**
 * The function __system_task_start initializes a system task by assigning the system, creating a mutex
 * to stop the task, and assigning the task arguments.
 *
 * @param sys A pointer to the system structure that the task belongs to.
 * @param task The "task" parameter is a pointer to a structure of type "system_task_t". This structure
 * contains information about the task, such as its system and arguments.
 * @param args The "args" parameter is a void pointer that can be used to pass any additional arguments
 * or data to the task. It can be cast to the appropriate data type inside the task function to access
 * the passed arguments.
 */
void system_task_start(system_t *sys, system_task_t *task, TaskFunction_t function, const char *const name,
                       configSTACK_DEPTH_TYPE stack_depth, void *args, UBaseType_t priority);

/**
 * The function `system_task_start_in_core` starts a system task on a specific core in FreeRTOS.
 *
 * @param sys A pointer to the system structure that contains information about the system.
 * @param task A pointer to a system_task_t structure, which holds information about the task being
 * created.
 * @param function The function pointer to the task function that will be executed when the task is
 * started.
 * @param name The name of the task. It is a string that identifies the task and is used for debugging
 * and logging purposes.
 * @param stack_depth The stack depth parameter specifies the size of the stack allocated for the task.
 * It determines how much memory is reserved for the task to store its local variables and function
 * call stack. The value is specified in units of bytes.
 * @param args args is a pointer to any arguments that need to be passed to the task function. These
 * arguments can be of any data type and can be used by the task function to perform its operations.
 * @param priority The priority parameter is used to specify the priority level of the task being
 * created. It determines the order in which tasks are executed when there are multiple tasks ready to
 * run. The higher the priority value, the higher the priority of the task.
 * @param coreid The "coreid" parameter is used to specify the core on which the task should be pinned.
 * In a multi-core system, each core can execute tasks independently. By specifying the coreid, you can
 * control on which core the task will run. The valid values for coreid depend on the specific
 */
void system_task_start_in_core(system_t *sys, system_task_t *task, TaskFunction_t function, const char *const name,
                               configSTACK_DEPTH_TYPE stack_depth, void *args, UBaseType_t priority, BaseType_t coreid);

// system task stop
/**
 * The function stops a system task and deletes its handler and associated resources.
 *
 * @param sys A pointer to the system structure that contains the task.
 * @param task A pointer to the system_task_t structure representing the task to be stopped.
 * @param timeout_ms The timeout_ms parameter is the maximum amount of time, in milliseconds, that the
 * function will wait for the task to stop before timing out.
 */
void system_task_stop(system_t *sys, system_task_t *task, uint16_t timeout_ms);

#define system_task_alive(sys, task) ((task)->system == (sys))

// macros to develop the state machine system
#define STATE_MACHINE(sys)                                                     \
    while (1) {                                                                \
        if (xSemaphoreTake(sys.sys_new_state, pdMS_TO_TICKS(100)) == pdTRUE) { \
            switch (sys.sys_state)

#define STATE_MACHINE_BEGIN()

#define STATE(x) case x:

#define STATE_BEGIN()

#define STATE_END() break

#define STATE_MACHINE_END() \
    }                       \
    }

// macros to develop tasks
#define SYSTEM_TASK(fn) void fn(void *__ptr)

#define TASK_BEGIN() system_task_t *__task = (system_task_t *)__ptr

#define TASK_END()                         \
    xSemaphoreGive(__task->sys_task_stop); \
    vTaskDelay(pdMS_TO_TICKS(50));         \
    while (1) vTaskDelay(pdMS_TO_TICKS(50))
// vTaskDelete(NULL)

#define TASK_ARGS __task->sys_task_args

#define TASK_LOOP() while (uxSemaphoreGetCount(__task->sys_task_stop))

// macros to switch state from a task
#define SWITCH_ST_FROM_TASK(new_st) esp_event_post_to(__task->system->sys_evt_loop, (esp_event_base_t)__task->system->sys_id, new_st, NULL, 0, portMAX_DELAY)
#define SWITCH_ST(sys, new_st) esp_event_post_to((sys)->sys_evt_loop, (esp_event_base_t)(sys)->sys_id, new_st, NULL, 0, portMAX_DELAY)

#define GET_ST_FROM_TASK() __task->system->sys_state

#endif
/**
 * @file debug_printf.c
 * @brief Debug printf implementation for STM32 with FreeRTOS
 */

#include "debug_printf.hpp"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#if DEBUG_PRINTF_USE_MUTEX
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#endif

/* ============================================================================
 * PRIVATE VARIABLES
 * ============================================================================ */

static UART_HandleTypeDef* s_debugUart = NULL;
static uint8_t s_enabled = 1;

#if DEBUG_PRINTF_USE_MUTEX
static SemaphoreHandle_t s_mutex = NULL;
static StaticSemaphore_t s_mutexBuffer;  /* Static allocation */
#endif

static char s_buffer[DEBUG_PRINTF_BUFFER_SIZE];

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

static inline void lock(void) {
#if DEBUG_PRINTF_USE_MUTEX
    if (s_mutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
#endif
}

static inline void unlock(void) {
#if DEBUG_PRINTF_USE_MUTEX
    if (s_mutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        xSemaphoreGive(s_mutex);
    }
#endif
}

static int transmit(const char* data, int len) {
    if (s_debugUart == NULL || len <= 0) {
        return 0;
    }

    /* Use blocking transmit - simple and reliable for debug */
    //HAL_UART_Transmit(s_debugUart, (uint8_t*)data, (uint16_t)len, HAL_MAX_DELAY);
    HAL_UART_Transmit_IT(s_debugUart, (uint8_t*)data, (uint16_t)len);
    return len;
}



/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

void DebugPrintf_Init(UART_HandleTypeDef* huart) {
    s_debugUart = huart;
    s_enabled = 1;
    
#if DEBUG_PRINTF_USE_MUTEX
    /* Create mutex using static allocation (no heap needed) */
    s_mutex = xSemaphoreCreateMutexStatic(&s_mutexBuffer);
#endif
}

void DebugPrintf_SetUart(UART_HandleTypeDef* huart) {
    lock();
    s_debugUart = huart;
    unlock();
}

UART_HandleTypeDef* DebugPrintf_GetUart(void) {
    return s_debugUart;
}

void DebugPrintf_Enable(uint8_t enable) {
    s_enabled = enable ? 1 : 0;
}

uint8_t DebugPrintf_IsEnabled(void) {
    return s_enabled;
}

int DebugPrintf(const char* format, ...) {
#if !DEBUG_PRINTF_ENABLED
    (void)format;
    return 0;
#else
    if (!s_enabled || s_debugUart == NULL) {
        return 0;
    }
    
    lock();
    
    int offset = 0;
    
#if DEBUG_PRINTF_TIMESTAMP
    /* Add timestamp [XXXXX.XXX] in milliseconds */
    uint32_t tick = HAL_GetTick();
    offset = snprintf(s_buffer, DEBUG_PRINTF_BUFFER_SIZE, 
                      "[%5lu.%03lu] ", 
                      tick / 1000, tick % 1000);
#endif

#if DEBUG_PRINTF_TASKNAME
    /* Add task name if running in a task context */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        TaskHandle_t task = xTaskGetCurrentTaskHandle();
        if (task != NULL) {
            const char* name = pcTaskGetName(task);
            if (name != NULL) {
                offset += snprintf(s_buffer + offset, 
                                   DEBUG_PRINTF_BUFFER_SIZE - offset,
                                   "[%-10s] ", name);
            }
        }
    }
#endif
    
    /* Format the user message */
    va_list args;
    va_start(args, format);
    int len = vsnprintf(s_buffer + offset, 
                        DEBUG_PRINTF_BUFFER_SIZE - offset, 
                        format, args);
    va_end(args);
    
    if (len < 0) {
        unlock();
        return 0;
    }
    
    int total = offset + len;
    if (total > DEBUG_PRINTF_BUFFER_SIZE - 1) {
        total = DEBUG_PRINTF_BUFFER_SIZE - 1;
    }
    
    int result = transmit(s_buffer, total);
    
    unlock();
    return result;
#endif
}

int DebugPrintfLevel(const char* level, const char* format, ...) {
#if !DEBUG_PRINTF_ENABLED
    (void)level;
    (void)format;
    return 0;
#else
    if (!s_enabled || s_debugUart == NULL) {
        return 0;
    }
    
    lock();
    
    int offset = 0;
    
#if DEBUG_PRINTF_TIMESTAMP
    uint32_t tick = HAL_GetTick();
    offset = snprintf(s_buffer, DEBUG_PRINTF_BUFFER_SIZE,
                      "[%5lu.%03lu] ", 
                      tick / 1000, tick % 1000);
#endif

    /* Add level tag */
    offset += snprintf(s_buffer + offset, 
                       DEBUG_PRINTF_BUFFER_SIZE - offset,
                       "[%s] ", level);

#if DEBUG_PRINTF_TASKNAME
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        TaskHandle_t task = xTaskGetCurrentTaskHandle();
        if (task != NULL) {
            const char* name = pcTaskGetName(task);
            if (name != NULL) {
                offset += snprintf(s_buffer + offset,
                                   DEBUG_PRINTF_BUFFER_SIZE - offset,
                                   "[%-10s] ", name);
            }
        }
    }
#endif
    
    va_list args;
    va_start(args, format);
    int len = vsnprintf(s_buffer + offset,
                        DEBUG_PRINTF_BUFFER_SIZE - offset,
                        format, args);
    va_end(args);
    
    if (len < 0) {
        unlock();
        return 0;
    }
    
    int total = offset + len;
    if (total > DEBUG_PRINTF_BUFFER_SIZE - 1) {
        total = DEBUG_PRINTF_BUFFER_SIZE - 1;
    }
    
    int result = transmit(s_buffer, total);
    
    unlock();
    return result;
#endif
}

void DebugPrintf_HexDump(const char* prefix, const uint8_t* data, uint16_t length) {
#if !DEBUG_PRINTF_ENABLED
    (void)prefix;
    (void)data;
    (void)length;
    return;
#else
    if (!s_enabled || s_debugUart == NULL || data == NULL) {
        return;
    }
    
    lock();
    
    int offset = 0;
    
#if DEBUG_PRINTF_TIMESTAMP
    uint32_t tick = HAL_GetTick();
    offset = snprintf(s_buffer, DEBUG_PRINTF_BUFFER_SIZE,
                      "[%5lu.%03lu] ", 
                      tick / 1000, tick % 1000);
#endif
    
    /* Print prefix and byte count */
    offset += snprintf(s_buffer + offset,
                       DEBUG_PRINTF_BUFFER_SIZE - offset,
                       "%s (%u bytes): ", 
                       prefix ? prefix : "HEX", length);
    
    /* Print hex bytes */
    for (uint16_t i = 0; i < length && offset < DEBUG_PRINTF_BUFFER_SIZE - 4; i++) {
        offset += snprintf(s_buffer + offset,
                           DEBUG_PRINTF_BUFFER_SIZE - offset,
                           "%02X ", data[i]);
    }
    
    /* Add newline */
    if (offset < DEBUG_PRINTF_BUFFER_SIZE - 2) {
        s_buffer[offset++] = '\r';
        s_buffer[offset++] = '\n';
    }
    
    transmit(s_buffer, offset);
    
    unlock();
#endif
}

/* ============================================================================
 * OPTIONAL: Override _write for printf() redirection
 * Uncomment if you want regular printf() to also use this system
 * ============================================================================ */
#if 0
int _write(int file, char *ptr, int len) {
    (void)file;
    if (s_debugUart != NULL && len > 0) {
        HAL_UART_Transmit(s_debugUart, (uint8_t*)ptr, (uint16_t)len, HAL_MAX_DELAY);
    }
    return len;
}
#endif

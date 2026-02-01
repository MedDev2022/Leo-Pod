/**
 * @file debug_printf.h
 * @brief Debug printf utility for STM32 with FreeRTOS
 * 
 * Features:
 * - Compile-time enable/disable via DEBUG_PRINTF_ENABLED
 * - Runtime UART redirection
 * - Thread-safe (uses FreeRTOS mutex)
 * - Optional timestamp prefix
 * - Optional task name prefix
 * - Zero overhead when disabled
 */

#ifndef DEBUG_PRINTF_H
#define DEBUG_PRINTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdarg.h>

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/** Enable/disable debug output at compile time (1 = enabled, 0 = disabled) */
#ifndef DEBUG_PRINTF_ENABLED
#define DEBUG_PRINTF_ENABLED    1
#endif

/** Include timestamp in debug output */
#ifndef DEBUG_PRINTF_TIMESTAMP
#define DEBUG_PRINTF_TIMESTAMP  1
#endif

/** Include task name in debug output */
#ifndef DEBUG_PRINTF_TASKNAME
#define DEBUG_PRINTF_TASKNAME   0
#endif

/** Maximum length of a single debug message */
#ifndef DEBUG_PRINTF_BUFFER_SIZE
#define DEBUG_PRINTF_BUFFER_SIZE 256
#endif

/** Use mutex for thread safety (recommended with FreeRTOS) */
#ifndef DEBUG_PRINTF_USE_MUTEX
#define DEBUG_PRINTF_USE_MUTEX  1
#endif

/* ============================================================================
 * API FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize debug printf system
 * @param huart Pointer to UART handle for debug output
 * @note Call this after FreeRTOS scheduler starts if using mutex
 */
void DebugPrintf_Init(UART_HandleTypeDef* huart);

/**
 * @brief Set/change the debug UART at runtime
 * @param huart Pointer to new UART handle (NULL to disable output)
 */
void DebugPrintf_SetUart(UART_HandleTypeDef* huart);

/**
 * @brief Get current debug UART handle
 * @return Current UART handle or NULL if not set
 */
UART_HandleTypeDef* DebugPrintf_GetUart(void);

/**
 * @brief Enable or disable debug output at runtime
 * @param enable 1 to enable, 0 to disable
 */
void DebugPrintf_Enable(uint8_t enable);

/**
 * @brief Check if debug output is enabled
 * @return 1 if enabled, 0 if disabled
 */
uint8_t DebugPrintf_IsEnabled(void);

/**
 * @brief Print formatted debug message
 * @param format Printf-style format string
 * @param ... Variable arguments
 * @return Number of characters printed, or 0 if disabled/error
 */
int DebugPrintf(const char* format, ...);

/**
 * @brief Print formatted debug message with explicit level
 * @param level Debug level string (e.g., "INFO", "WARN", "ERR")
 * @param format Printf-style format string
 * @param ... Variable arguments
 * @return Number of characters printed
 */
int DebugPrintfLevel(const char* level, const char* format, ...);

/**
 * @brief Print hex dump of data
 * @param prefix Label to print before hex dump
 * @param data Pointer to data
 * @param length Number of bytes to dump
 */
void DebugPrintf_HexDump(const char* prefix, const uint8_t* data, uint16_t length);

/* ============================================================================
 * CONVENIENCE MACROS
 * ============================================================================ */

#if DEBUG_PRINTF_ENABLED

    /** Standard debug print */
    #define DBG(fmt, ...)       DebugPrintf(fmt, ##__VA_ARGS__)
    
    /** Debug print with newline */
    #define DBGLN(fmt, ...)     DebugPrintf(fmt "\r\n", ##__VA_ARGS__)
    
    /** Info level */
    #define DBG_INFO(fmt, ...)  DebugPrintfLevel("INFO", fmt "\r\n", ##__VA_ARGS__)
    
    /** Warning level */
    #define DBG_WARN(fmt, ...)  DebugPrintfLevel("WARN", fmt "\r\n", ##__VA_ARGS__)
    
    /** Error level */
    #define DBG_ERR(fmt, ...)   DebugPrintfLevel("ERR ", fmt "\r\n", ##__VA_ARGS__)
    
    /** Hex dump */
    #define DBG_HEX(prefix, data, len)  DebugPrintf_HexDump(prefix, data, len)
    
    /** Debug print with file and line */
    #define DBG_TRACE(fmt, ...) DebugPrintf("[%s:%d] " fmt "\r\n", __FILE__, __LINE__, ##__VA_ARGS__)

#else
    /* All macros compile to nothing when disabled */
    #define DBG(fmt, ...)               ((void)0)
    #define DBGLN(fmt, ...)             ((void)0)
    #define DBG_INFO(fmt, ...)          ((void)0)
    #define DBG_WARN(fmt, ...)          ((void)0)
    #define DBG_ERR(fmt, ...)           ((void)0)
    #define DBG_HEX(prefix, data, len)  ((void)0)
    #define DBG_TRACE(fmt, ...)         ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_PRINTF_H */
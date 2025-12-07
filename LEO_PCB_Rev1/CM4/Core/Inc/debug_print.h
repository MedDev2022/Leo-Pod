/**
 ******************************************************************************
 * @file    debug_print.h
 * @brief   Global non-blocking debug print system
 * @author  MK Medical DEvice Solutions Ltd
 * @date    2025
 ******************************************************************************
 * @attention
 * 
 * Usage:
 *   1. Call SetDebugOutput(endpoint) once during initialization
 *   2. Use debug_printf() anywhere for non-blocking output
 *   3. Standard printf() will also be non-blocking
 * 
 * Example:
 *   SetDebugOutput(g_cli);
 *   debug_printf("System ready\r\n");
 *   printf("Heap: %u\r\n", xPortGetFreeHeapSize());
 * 
 ******************************************************************************
 */

#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

// Forward declaration
class UartEndpoint;

// Global debug output endpoint
static UartEndpoint* g_debugOutput = nullptr;

/**
 * @brief Set the global debug output UART endpoint
 * @param endpoint Pointer to UartEndpoint (Host, CLI, etc.) or NULL to disable
 */
void SetDebugOutput(UartEndpoint* endpoint);

/**
 * @brief Get the current debug output endpoint
 * @return Pointer to current debug endpoint, or NULL if not set
 */
UartEndpoint* GetDebugOutput(void);

/**
 * @brief Non-blocking printf to debug output
 * @param format Printf-style format string
 * @param ... Variable arguments
 * @return Number of characters queued, or -1 if error or no output set
 * 
 * @note This function is safe to call from any task
 * @note If SetDebugOutput() was not called, this returns -1
 */
int debug_printf(const char* format, ...);

/**
 * @brief Non-blocking write to debug output
 * @param data Pointer to data buffer
 * @param length Number of bytes to write
 * @return Number of bytes queued, or -1 if error or no output set
 */
int debug_write(const uint8_t* data, size_t length);

/**
 * @brief Check if debug output is configured
 * @return true if debug output is set and ready
 */
bool debug_is_ready(void);

/**
 * @brief Get debug TX queue statistics
 * @param used Output: bytes currently in TX queue
 * @param capacity Output: total TX queue capacity
 * @return true if statistics retrieved successfully
 */
bool debug_get_stats(uint32_t* used, uint32_t* capacity);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_PRINT_H */

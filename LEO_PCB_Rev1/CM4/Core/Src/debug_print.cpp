/**
 ******************************************************************************
 * @file    debug_print.cpp
 * @brief   Global non-blocking debug print system implementation
 ******************************************************************************
 */

#include "debug_print.h"
#include "UartEndpoint.hpp"
#include <cstdio>
#include <cstring>

// Global debug output endpoint
static UartEndpoint* g_debugOutput = nullptr;

/**
 * @brief Set the global debug output UART endpoint
 */
extern "C" void SetDebugOutput(UartEndpoint* endpoint) {
    g_debugOutput = endpoint;
}

/**
 * @brief Get the current debug output endpoint
 */
extern "C" UartEndpoint* GetDebugOutput(void) {
    return g_debugOutput;
}

/**
 * @brief Non-blocking printf to debug output
 */
extern "C" int debug_printf(const char* format, ...) {
    if (g_debugOutput == nullptr) {
        return -1;  // No debug output configured
    }
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (length < 0) {
        return -1;
    }
    
    if (length >= (int)sizeof(buffer)) {
        length = sizeof(buffer) - 1;
    }
    
    return g_debugOutput->write((uint8_t*)buffer, length);
}

/**
 * @brief Non-blocking write to debug output
 */
extern "C" int debug_write(const uint8_t* data, size_t length) {
    if (g_debugOutput == nullptr) {
        return -1;
    }
    
    return g_debugOutput->write(data, length);
}

/**
 * @brief Check if debug output is configured
 */
extern "C" bool debug_is_ready(void) {
    return (g_debugOutput != nullptr);
}

/**
 * @brief Get debug TX queue statistics
 */
extern "C" bool debug_get_stats(uint32_t* used, uint32_t* capacity) {
    if (g_debugOutput == nullptr || used == nullptr || capacity == nullptr) {
        return false;
    }
    
    g_debugOutput->getTxStats(*used, *capacity);
    return true;
}

/**
 * @brief Override standard _write for printf redirection
 * @note This makes standard printf() non-blocking when debug output is set
 */
extern "C" int _write(int file, char *ptr, int len) {
    (void)file;  // Unused

    if (g_debugOutput != nullptr) {
        return g_debugOutput->write((uint8_t*)ptr, len);
    }

    // Fallback: If no debug output is set, use blocking UART
    // You can customize this fallback behavior
    extern UART_HandleTypeDef huart4;
    HAL_UART_Transmit_IT(&huart4, (uint8_t*)ptr, len);
    return len;
}

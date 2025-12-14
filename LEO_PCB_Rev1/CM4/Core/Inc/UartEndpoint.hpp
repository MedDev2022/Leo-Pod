#pragma once

#include "main.h"
#include <cstdint>
#include <map>
#include <deque>
#include "cmsis_os2.h"
#include <cstdarg>
#include <cstdio>


enum class DevCommMode {
    Normal,			//Parse messages
	Transparent		//Raw forwarding
};

class UartEndpoint {
public:

	// Debug counters - add these if not present
	volatile uint32_t dbg_txStartCount = 0;
	volatile uint32_t dbg_txCompleteCount = 0;
	volatile uint32_t dbg_txFailCount = 0;

	explicit UartEndpoint(UART_HandleTypeDef* huart, const char* taskName = "UartTask");
	virtual ~UartEndpoint();

    bool StartReceive();

    bool SetBaudrate(uint32_t baudrate);

    uint32_t GetBaudrate() const { return huart_->Init.BaudRate; }

    uint16_t SendCommand(const uint8_t* command, size_t length);

    // Pure virtual - derived classes must implement
    virtual void processRxData(uint8_t byte) = 0;

    // Add to UartEndpoint.hpp (public section):
    bool testBlockingTx(const uint8_t* data, size_t length);

    // ============================================================
    // NON-BLOCKING PRINTF FUNCTIONALITY
    // ============================================================

    /**
     * @brief Non-blocking printf - queues formatted string for transmission
     * @param format Printf-style format string
     * @param ... Variable arguments
     * @return Number of characters queued, or -1 on error
     */
//    int printf(const char* format, ...);

    /**
     * @brief Non-blocking write - queues raw data for transmission
     * @param data Pointer to data buffer
     * @param length Number of bytes to send
     * @return Number of bytes queued, or -1 on error
     */
    int write(const uint8_t* data, size_t length);

    /**
     * @brief Check if TX queue has space
     * @return Number of free bytes in TX queue
     */
    uint32_t getTxSpace() const;

    uint32_t getRxSpace() const;

    /**
     * @brief Check if TX is busy
     * @return true if transmission in progress
     */
    bool isTxBusy() const { return txBusy_; }

    /**
      * @brief Get TX queue usage statistics
      * @param used Output: bytes currently queued
      * @param capacity Output: total queue capacity
      */
     void getTxStats(uint32_t& used, uint32_t& capacity) const;

    // Transparent mode
    void setTransparentMode(bool enable, UartEndpoint* destination = nullptr);
    bool isTransparentMode() const { return commMode_ == DevCommMode::Transparent; }

    UART_HandleTypeDef* huart_;

    // ISR callback dispatcher
    static void DispatchRxComplete(UART_HandleTypeDef* huart);
    static void DispatchTxComplete(UART_HandleTypeDef* huart);

protected:

    // Task entry point (static wrapper)
    static void TaskEntry(void* argument);

    // Actual task loop (virtual, can be overridden)
    virtual void taskLoop();

    // Queue for received bytes (ISR → Task)
    osMessageQueueId_t rxQueue_;

    // Queue for transmit bytes (write() → ISR)
    osMessageQueueId_t txQueue_;

    // Transparent mode settings
    DevCommMode commMode_ = DevCommMode::Normal;
    UartEndpoint* destEndpoint_ = nullptr;

    uint32_t baudrate_;

private:


    // ============================================================
    // TX QUEUE SYSTEM
    // ============================================================

    static constexpr size_t TX_QUEUE_SIZE = 2048;  // Adjust as needed
    static constexpr size_t RX_QUEUE_SIZE = 2048;  // Adjust as needed


    uint8_t txByte_;  // Current byte being transmitted
    // TX state tracking
    volatile bool txBusy_;
    volatile bool txPending_;  // Flag to signal task to continue TX

    // Semaphore for TX notification (optional, for task-based TX)
    osSemaphoreId_t txSemaphore_;

    // Start transmission if idle
    void startNextTransmission();

    // RX members
    uint8_t rxByte_;  // Single byte buffer for ISR
    osThreadId_t taskHandle_;
    const char* taskName_;

    static std::map<UART_HandleTypeDef*, UartEndpoint*> instanceMap;


};

// ISR (C-linkage)
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart);
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart);




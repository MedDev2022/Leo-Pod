#pragma once

#include "main.h"
#include <cstdint>
#include <map>
#include "cmsis_os2.h"
#include "stream_buffer.h"


enum class DevCommMode {
    Normal,			//Parse messages
	Transparent		//Raw forwarding
};

class UartEndpoint {
public:

	explicit UartEndpoint(UART_HandleTypeDef* huart, const char* taskName = "UartTask");
	virtual ~UartEndpoint();

    bool StartReceive();
    bool ReStartReceive();
    bool SetBaudrate(uint32_t baudrate);
    uint32_t GetBaudrate() const { return huart_->Init.BaudRate; }


    uint16_t SendCommand(const uint8_t* command, size_t length);

    // Non-blocking write - appends to TX stream, callable from anywhere
    int write(const uint8_t* data, size_t length);

    // Pure virtual - derived classes must implement
    virtual void processRxData(const uint8_t* data, uint16_t length) = 0;

    // Transparent mode
    void setTransparentMode(bool enable, UartEndpoint* destination = nullptr);
    bool isTransparentMode() const { return commMode_ == DevCommMode::Transparent; }

    // ISR dispatchers
    static void DispatchRxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
    static void DispatchTxComplete(UART_HandleTypeDef* huart);

    // Encryption helpers
    static void EncryptInPlace(unsigned char* data, int length);
    static void DecryptInPlace(unsigned char* data, int length);

    UART_HandleTypeDef* huart_;
    UartEndpoint* destEndpointW_ = nullptr;

protected:
    DevCommMode commMode_ = DevCommMode::Normal;
    UartEndpoint* destEndpoint_ = nullptr;
    uint32_t baudrate_;

private:
    static constexpr size_t RX_BUFFER_SIZE = 256;
    static constexpr size_t TX_BUFFER_SIZE = 512;
    static constexpr size_t TX_CHUNK_SIZE = 64;

    // Stream buffers
    StreamBufferHandle_t rxStream_;
    StreamBufferHandle_t txStream_;

    // Task handles
    osThreadId_t rxTaskHandle_;
    osThreadId_t txTaskHandle_;
    const char* taskName_;

    // RX buffer for ISR
    uint8_t rxBuffer_[RX_BUFFER_SIZE];

    // TX buffer for interrupt transmission
    uint8_t txChunk_[TX_CHUNK_SIZE];

    // Task loops
    void rxTaskLoop();
    void txTaskLoop();
    static void RxTaskEntry(void* arg);
    static void TxTaskEntry(void* arg);

    static std::map<UART_HandleTypeDef*, UartEndpoint*> instanceMap;
};

// ISR (C-linkage)
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart);

// Device IDs
static constexpr uint8_t LEOPOD_ID 	= 0x70;
static constexpr uint8_t HOST_ID 	= 0x10;
static constexpr uint8_t DAYCAM_ID 	= 0x21;
static constexpr uint8_t IRAY_ID 	= 0x22;
static constexpr uint8_t RPLENS_ID 	= 0x23;
static constexpr uint8_t LRF_ID 	= 0x24;

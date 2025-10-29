#pragma once

#include "main.h"
#include <cstdint>
#include <map>
#include <deque>
#include "cmsis_os2.h"


enum class DevCommMode {
    Normal,			//Parse messages
	Transparent		//Raw forwarding
};

class UartEndpoint {
public:

	explicit UartEndpoint(UART_HandleTypeDef* huart, const char* taskName = "UartTask");
	virtual ~UartEndpoint();

    bool StartReceive();
    uint16_t SendCommand(const uint8_t* command, size_t length);

    // Pure virtual - derived classes must implement
    virtual void processRxData(uint8_t byte) = 0;


    // Transparent mode
    void setTransparentMode(bool enable, UART_HandleTypeDef* destination = nullptr);
    bool isTransparentMode() const { return commMode_ == DevCommMode::Transparent; }

    UART_HandleTypeDef* huart_;

    // ISR callback dispatcher
    static void DispatchRxComplete(UART_HandleTypeDef* huart);

protected:
    // Task entry point (static wrapper)
    static void TaskEntry(void* argument);

    // Actual task loop (virtual, can be overridden)
    virtual void taskLoop();

    // Queue for received bytes (ISR → Task)
    osMessageQueueId_t rxQueue_;

    // Transparent mode settings
    DevCommMode commMode_ = DevCommMode::Normal;
    UART_HandleTypeDef* destHuart_ = nullptr;


private:
    uint8_t rxByte_;  // Single byte buffer for ISR
    osThreadId_t taskHandle_;
    const char* taskName_;

    static std::map<UART_HandleTypeDef*, UartEndpoint*> instanceMap;
};

// ISR (C-linkage)
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart);


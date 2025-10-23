#pragma once

#include "main.h"
#include <cstdint>
#include <map>
#include <deque>


class UartEndpoint {
public:
    explicit UartEndpoint(UART_HandleTypeDef* huart);
    bool StartReceive(uint8_t* buffer, size_t len);

    virtual void onReceiveByte(uint8_t byte) = 0;  // abstract function
    virtual void processIncoming();            // derived classes override
    static void DispatchRxComplete(UART_HandleTypeDef* huart);

    uint16_t SendCommand(const uint8_t* command, size_t length);

protected:
    UART_HandleTypeDef* huart_;
    uint8_t* rxBuffer_;
    size_t rxLength_;

    std::deque<uint8_t> rxQueue_;  // ⬅️ byte queue for streaming data

    static std::map<UART_HandleTypeDef*, UartEndpoint*> instanceMap;
};

// ISR (C-linkage)
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart);

#pragma once
#include "UartEndpoint.hpp"

class CLI : public UartEndpoint {
public:
    explicit CLI(UART_HandleTypeDef* huart, uint32_t baudrate = 115200);
    void Init();

protected:
    size_t processRxData(const uint8_t* data, size_t length) override;
};

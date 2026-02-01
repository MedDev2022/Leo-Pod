#pragma once
#include "UartEndpoint.hpp"

class CLI : public UartEndpoint {
public:
    explicit CLI(UART_HandleTypeDef* huart, uint32_t baudrate = 115200);
    void Init();

protected:
    void processRxData(const uint8_t* data, uint16_t length) override;
};

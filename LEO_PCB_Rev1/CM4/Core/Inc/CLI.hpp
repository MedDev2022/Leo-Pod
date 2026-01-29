#pragma once
#include "UartEndpoint.hpp"
#include <string>
#include <vector>

class CLI : public UartEndpoint {
public:
    explicit CLI(UART_HandleTypeDef* huart, uint32_t baudrate = 115200);
    void Init();



private:

    uint8_t rxBuffer[64];  // Adjust size as necessary for your data
    uint8_t byte_;
protected:
//    void onReceiveByte(uint8_t byte) override;
//    void processIncoming() override;
    // Override the task-based processing
    void processRxData(const uint8_t* data, uint16_t length) override;

};

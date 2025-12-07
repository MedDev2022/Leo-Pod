#pragma once
#include "UartEndpoint.hpp"
#include <string>
#include <vector>
#include <deque>
#include "main.h"

#define VISCA_ADDRESS 0x01
#define VISCA_BAUD_RATE 9600

class DayCam : public UartEndpoint {
public:
    explicit DayCam(UART_HandleTypeDef* huart, uint32_t baudrate = 9600);
    void Init();

    void setAddress();
    void setProtocol();
    void ifClear();

    void handleZoomIn(uint8_t* speed, uint8_t length);
    void handleZoomOut(uint8_t* speed, uint8_t length);
    void handleZoom2Position(uint16_t position);
    void handleZoomStop();

    void handleFocusFar(uint8_t* speed, uint8_t length);
    void handleFocusNear(uint8_t* speed, uint8_t length);
    void handleFocus2Position(uint16_t position);
    void handleFocusStop();

    // Command arrays
    uint8_t address_command[4] = { 0x88, 0x30, 0x01, 0xFF };
    uint8_t if_clear[5] = { 0x88, 0x01, 0x00, 0x01, 0xFF };
    uint8_t zoom_teleVar[6] = { 0x81, 0x01, 0x04, 0x07, 0x25, 0xFF };
    uint8_t zoom_wideVar[6] = { 0x81, 0x01, 0x04, 0x07, 0x35, 0xFF };
    uint8_t zoom_stop[6] = { 0x81, 0x01, 0x04, 0x07, 0x00, 0xFF };
    uint8_t zoom2Position[9] = { 0x81, 0x01, 0x04, 0x47, 0x00, 0x00, 0x00, 0x00, 0xFF };
    uint8_t focus_farVar[6] = { 0x81, 0x01, 0x04, 0x08, 0x25, 0xFF };
    uint8_t focus_nearVar[6] = { 0x81, 0x01, 0x04, 0x08, 0x35, 0xFF };
    uint8_t focus_stop[6] = { 0x81, 0x01, 0x04, 0x08, 0x00, 0xFF };
    uint8_t focus2Position[9] = { 0x81, 0x01, 0x04, 0x48, 0x00, 0x00, 0x00, 0x00, 0xFF };

protected:
    // Override the task-based processing
    void processRxData(uint8_t byte) override;



private:
    std::deque<uint8_t> messageBuffer_;  // Buffer for building VISCA messages
};

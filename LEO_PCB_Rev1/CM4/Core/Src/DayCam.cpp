#include "DayCam.hpp"
#include <cstdio>
#include <cstring>

DayCam::DayCam(UART_HandleTypeDef* huart)
    : UartEndpoint(huart, "DayCamTask") {}

void DayCam::Init() {
    setProtocol();
    osDelay(1000);

    setAddress();
    osDelay(1000);

    ifClear();
    osDelay(1000);

    if (!StartReceive()) {
        printf("DayCam Start Receive failed\r\n");
    } else {
        printf("DayCam Start Receive success\r\n");
    }
}


void DayCam::processRxData(uint8_t byte) {
   // uint8_t byte;

    // Handle transparent mode (shouldn't reach here, but just in case)
    if (destHuart_ != nullptr) {
        HAL_UART_Transmit(destHuart_, &byte, 1, 100);
        return;
    }

//    // Read all available bytes from queue
//    while (osMessageQueueGet(rxQueue_, &byte, nullptr, 0) == osOK) {
//        messageBuffer_.push_back(byte);
//
//        // VISCA messages end with 0xFF
//        if (byte == 0xFF) {
//            // Process complete message
//            printf("DayCam RX: ");
//            for (uint8_t b : messageBuffer_) {
//                printf("0x%02X ", b);
//            }
//            printf("\r\n");
//
//            // TODO: Parse and handle VISCA response here
//
//            messageBuffer_.clear();
//        }
//
//        // Prevent buffer overflow
//        if (messageBuffer_.size() > 64) {
//            printf("DayCam: Message buffer overflow, clearing\r\n");
//            messageBuffer_.clear();
//        }
//    }
}

void DayCam::setProtocol() {
    const uint8_t command[] = {0x30, 0x01, 0x00};
    SendCommand(command, 3);
}

void DayCam::ifClear() {
    const uint8_t command[] = {0x88, 0x01, 0x00, 0x01, 0xFF};
    SendCommand(command, 5);
}

void DayCam::setAddress() {
    SendCommand(address_command, 4);
}

void DayCam::handleZoomIn(uint8_t* speed, uint8_t length) {
    uint8_t temp_buff[sizeof(zoom_teleVar)];
    memcpy(temp_buff, zoom_teleVar, sizeof(zoom_teleVar));

    if (speed[0] > 0 && speed[0] < 8) {
        temp_buff[4] = ((temp_buff[4] & 0xF0) | (speed[0] & 0x0F));
        SendCommand(temp_buff, sizeof(temp_buff));
    }
}

void DayCam::handleZoomOut(uint8_t* speed, uint8_t length) {
    uint8_t temp_buff[sizeof(zoom_wideVar)];
    memcpy(temp_buff, zoom_wideVar, sizeof(zoom_wideVar));

    if (speed[0] > 0 && speed[0] < 8) {
        temp_buff[4] = ((temp_buff[4] & 0xF0) | (speed[0] & 0x0F));
        SendCommand(temp_buff, sizeof(temp_buff));
    }
}

void DayCam::handleZoom2Position(uint16_t position) {
    uint8_t temp_buff[sizeof(zoom2Position)];
    memcpy(temp_buff, zoom2Position, sizeof(zoom2Position));

    if (position <= 0x4000) {
        temp_buff[4] = (position & 0xF000) >> 12;
        temp_buff[5] = (position & 0x0F00) >> 8;
        temp_buff[6] = (position & 0x00F0) >> 4;
        temp_buff[7] = (position & 0x000F);
        SendCommand(temp_buff, sizeof(temp_buff));
    }
}

void DayCam::handleZoomStop() {
    SendCommand(zoom_stop, sizeof(zoom_stop));
}

void DayCam::handleFocusFar(uint8_t* speed, uint8_t length) {
    uint8_t temp_buff[sizeof(focus_farVar)];
    memcpy(temp_buff, focus_farVar, sizeof(focus_farVar));

    if (speed[0] > 0 && speed[0] < 8) {
        temp_buff[4] = ((temp_buff[4] & 0xF0) | (speed[0] & 0x0F));
        SendCommand(temp_buff, sizeof(temp_buff));
    }
}

void DayCam::handleFocusNear(uint8_t* speed, uint8_t length) {
    uint8_t temp_buff[sizeof(focus_nearVar)];
    memcpy(temp_buff, focus_nearVar, sizeof(focus_nearVar));

    if (speed[0] > 0 && speed[0] < 8) {
        temp_buff[4] = ((temp_buff[4] & 0xF0) | (speed[0] & 0x0F));
        SendCommand(temp_buff, sizeof(temp_buff));
    }
}

void DayCam::handleFocus2Position(uint16_t position) {
    uint8_t temp_buff[sizeof(focus2Position)];
    memcpy(temp_buff, focus2Position, sizeof(focus2Position));

    if (position <= 0x4000) {
        temp_buff[4] = (position & 0xF000) >> 12;
        temp_buff[5] = (position & 0x0F00) >> 8;
        temp_buff[6] = (position & 0x00F0) >> 4;
        temp_buff[7] = (position & 0x000F);
        SendCommand(temp_buff, sizeof(temp_buff));
    }
}

void DayCam::handleFocusStop() {
    SendCommand(focus_stop, sizeof(focus_stop));
}

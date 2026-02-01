#include "DayCam.hpp"
#include "Host.hpp"
#include <cstdio>
#include <cstring>

DayCam::DayCam(UART_HandleTypeDef* huart, uint32_t baudrate)
    : UartEndpoint(huart, "DayCamTask") {
	baudrate_ = baudrate;
}

void DayCam::Init() {
    if (huart_->Init.BaudRate != baudrate_) {
        SetBaudrate(baudrate_);
    }
    setProtocol();
    osDelay(1000);

    setAddress();
    osDelay(1000);

    ifClear();
    osDelay(1000);

    if (!ReStartReceive()) {
        printf("DayCam Start Receive failed\r\n");
    } else {
        printf("DayCam Start Receive success\r\n");
    }
}





// ============================================================================
// PROCESS DATA RECEIVED FROM RPLENS MOTOR CONTROLLER
// Encrypt and send back to Host for forwarding to external controller
// ============================================================================
void DayCam::processRxData(const uint8_t* data, uint16_t length) {
    if (data == nullptr || length == 0) {
    	printf("DayCam: bad length or data\r\n");
        return;
    }

    // Debug: Print raw received data
    printf("DayCam RX: %u bytes: ", length);
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");

    // Handle transparent mode - forward raw data
    if (commMode_ == DevCommMode::Transparent && destEndpoint_ != nullptr) {
        destEndpoint_->write(data, length);
        printf("DayCam: Sending response in transparent mode\r\n");
        return;
    }

    // Normal mode - send plain data to Host (Host will encrypt)
    if (destEndpointW_ != nullptr) {
        printf("DayCam: Sending response to Host\r\n");
        Host* host = static_cast<Host*>(destEndpointW_);
        host->sendDeviceResponse(DAYCAM_ID, data, length);  // Plain data - Host encrypts
    } else {
        printf("DayCam: No Host endpoint configured\r\n");
    }
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

/****************************************************************************
**  Name:    IRay Device Driver (inherits UartDevice)                      **
**  Author:  MK Medical Device Solutions Ltd.                             **
**  Website: www.mkmeddev.com                                             **
****************************************************************************/
#include "iRay.hpp"
#include <cstdio>
#include <iostream>
#include <cstring>
#include <queue>
#include "cmsis_os.h"
#include "comm.hpp"
#include "host.hpp"


IRay::IRay(UART_HandleTypeDef* huart, uint32_t baudrate)
    : UartEndpoint(huart, "iRayTask") {
	baudrate_ = baudrate;
}

void IRay::Init() {

    // Set baudrate before starting
    if (huart_->Init.BaudRate != baudrate_) {
        SetBaudrate(baudrate_);
    }

    if (huart_ == nullptr || huart_->Instance == nullptr) {
        printf("IRay: UART handle invalid!\r\n");
        return;
    }

    if (!StartReceive()) {
        printf("IRay receiver init failed\n");
    }
    else {
    	printf("IRay receiver init success\n");
    }

    uint8_t cmd1[] = {0xAA, 0x06, 0x01, 0x5D, 0x02, 0x04, 0x00, 0x14, 0xEB, 0xAA};
    this->SendCommand(cmd1, 10);

    uint8_t cmd2[] = {0xAA, 0x05, 0x01, 0x01, 0x01, 0x00, 0xB2, 0xEB, 0xAA};

    this->SendCommand(cmd2, 9);
}

void IRay::SetPalette(const std::string& palette) {
    if (irPalettes.find(palette) != irPalettes.end()) {
        SendCommand(irPalettes[palette].data(), irPalettes[palette].size());
    }
}

void IRay::setProtocol(){
	const uint8_t command[] = {0x76, 0x69, 0x64, 0x65, 0x6F, 0x20, 0x75, 0x74, 0x70, 0x75, 0x74, 0x20, 0x33, 0x0D};
    SendCommand(command, sizeof(command));
}


static constexpr uint8_t IRAY_REPLY_START = 0x55;
static constexpr uint8_t IRAY_END_0 = 0xEB;
static constexpr uint8_t IRAY_END_1 = 0xAA;
static constexpr size_t  IRAY_MIN_FRAME = 6;  // 0x55 + count + CW1 + OW + SC + 0xEB 0xAA

size_t IRay::processRxData(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return 0;
    }

    // Find start byte
    if (data[0] != IRAY_REPLY_START) {
        return 1;  // Not a start byte, skip to resync
    }

    // Need at least start + byte_count
    if (length < 2) {
        return 0;  // Wait for more
    }

    uint8_t bodyLen = data[1];
    size_t frameSize = bodyLen + 4;  // start + count + body + 0xEB + 0xAA

    if (frameSize < IRAY_MIN_FRAME) {
        return 1;  // Invalid, skip
    }

    // Wait for complete frame
    if (length < frameSize) {
        return 0;
    }

    // Verify end bytes
    if (data[frameSize - 2] != IRAY_END_0 || data[frameSize - 1] != IRAY_END_1) {
        printf("IRay: Bad end bytes, resyncing\r\n");
        return 1;  // Skip one byte, try resync
    }

    // Verify checksum (SC = sum of all body bytes before SC, mod 256)
    uint8_t sc = 0;
    for (size_t i = 2; i < frameSize - 3; i++) {  // CW1 through last RV
        sc += data[i];
    }
    if (sc != data[frameSize - 3]) {
        printf("IRay: Checksum mismatch\r\n");
        return 1;
    }

    // Valid frame - extract payload (skip start, count, and end bytes)
    const uint8_t* body = &data[2];

    printf("IRay RX: %u bytes, CW1=0x%02X\r\n", (unsigned)frameSize, body[0]);

    // Forward to Host
    if (destEndpointW_ != nullptr) {
        Host* host = static_cast<Host*>(destEndpointW_);
        host->sendDeviceResponse(IRAY_ID, data, frameSize);
    }

    return frameSize;
}
//
//// ============================================================================
//// PROCESS DATA RECEIVED FROM RPLENS MOTOR CONTROLLER
//// Encrypt and send back to Host for forwarding to external controller
//// ============================================================================
//size_t IRay::processRxData(const uint8_t* data, size_t length) {
//    if (data == nullptr || length == 0) {
//        return 0;
//    }
//
//    // Debug: Print raw received data
//    printf("IRay RX: %u bytes: ", length);
//    for (size_t i = 0; i < length; i++) {
//        printf("%02X ", data[i]);
//    }
//    printf("\r\n");
//
//    // Handle transparent mode - forward raw data
//    if (commMode_ == DevCommMode::Transparent && destEndpoint_ != nullptr) {
//        destEndpoint_->write(data, length);
//        return 0;
//    }
//
//    // Normal mode - send plain data to Host (Host will encrypt)
//    if (destEndpointW_ != nullptr) {
//        printf("IRay: Sending response to Host\r\n");
//        Host* host = static_cast<Host*>(destEndpointW_);
//        host->sendDeviceResponse(IRAY_ID, data, length);  // Plain data - Host encrypts
//    } else {
//        printf("IRay: No Host endpoint configured\r\n");
//    }
//}

void IRay::SetReticlePosition(int x, int y) {
    uint8_t cmd[] = {0xAA, 0x09, 0x01, 0x44, 0x02, 0x05, (uint8_t)x, (uint8_t)(x>>8), (uint8_t)y, (uint8_t)(y>>8), 0xC7, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::GetReticlePosition() {
    const uint8_t cmd[] = {0xAA, 0x04, 0x01, 0x44, 0x00, 0xF3, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::ReticleOn() {
    const uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0xc1, 0xb6, 0xeb, 0xaa};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::CalibReticleOFF() {
    const uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x40, 0x35, 0xeb, 0xaa};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::ReticleOFF() {
    const uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x00, 0xF5, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::Reticle1() {
    const uint8_t cmd[] = { 0xAA, 0x05, 0x01, 0x43, 0x02, 0x80, 0x75, 0xEB, 0xAA };
    SendCommand(cmd, sizeof(cmd));
}

void IRay::Reticle2() {
    const uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x81, 0x76, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::Reticle3() {
    const uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x82, 0x77, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::Reticle4() {
    const uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x83, 0x78, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::NUC_Shutter()
{
    const uint8_t cmd[] = {
        0xAA,
        0x05,
        0x01,
        0x11,
        0x02,
        0x01,  // Shutter NUC
        0xC4,
        0xEB, 0xAA
    };

    SendCommand(cmd, sizeof(cmd));
}


void IRay::InitializePalettes() {
    irPalettes["WH"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x00, 0xF4, 0xEB, 0xAA};
    irPalettes["BH"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x01, 0xF5, 0xEB, 0xAA};
    irPalettes["Rainbow"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x02, 0xF6, 0xEB, 0xAA};
    irPalettes["BGR"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x03, 0xF7, 0xEB, 0xAA};
    irPalettes["BRY"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x04, 0xF8, 0xEB, 0xAA};
}





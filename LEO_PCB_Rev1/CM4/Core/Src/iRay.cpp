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


IRay::IRay(UART_HandleTypeDef* huart)
    : UartEndpoint(huart, "iRayTask") {}

void IRay::Init() {
//    static uint8_t byte;
    if (!StartReceive()) {
        printf("IRay receiver init failed\n");
    }
    else {
    	printf("IRay receiver init success\n");
    }

}

void IRay::SetPalette(const std::string& palette) {
    if (irPalettes.find(palette) != irPalettes.end()) {
        SendCommand(irPalettes[palette].data(), irPalettes[palette].size());
    }
}

void IRay::processRxData(uint8_t byte) {
   // uint8_t byte;

    // Handle transparent mode (shouldn't reach here, but just in case)
    if (destHuart_ != nullptr) {
        HAL_UART_Transmit(destHuart_, &byte, 1, 100);
        return;
    }

}

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

void IRay::ReticleSetPosition() { //Place Reticle on x = 0x0064 and y = 0x0064
    const uint8_t cmd[] = {0xAA, 0x09, 0x01, 0x44, 0x02, 0x05, 0x64, 0x00, 0x64, 0x00, 0xC7, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::InitializePalettes() {
    irPalettes["WH"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x00, 0xF4, 0xEB, 0xAA};
    irPalettes["BH"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x01, 0xF5, 0xEB, 0xAA};
    irPalettes["Rainbow"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x02, 0xF6, 0xEB, 0xAA};
    irPalettes["BGR"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x03, 0xF7, 0xEB, 0xAA};
    irPalettes["BRY"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x04, 0xF8, 0xEB, 0xAA};
}





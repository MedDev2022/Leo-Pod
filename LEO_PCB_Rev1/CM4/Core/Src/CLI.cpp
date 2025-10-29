/****************************************************************************
**  Name:    IRay Device Driver (inherits UartDevice)                      **
**  Author:  MK Medical Device Solutions Ltd.                             **
**  Website: www.mkmeddev.com                                             **
****************************************************************************/
#include "CLI.hpp"
#include <cstdio>
#include <iostream>
#include <cstring>
#include <queue>
#include "cmsis_os.h"

CLI::CLI(UART_HandleTypeDef* huart)
    : UartEndpoint(huart) {}

void CLI::Init() {
//    static uint8_t byte;
    if (!StartReceive()) {
        printf("CLI receiver init failed\n");
    }
    else printf("CLI receiver init success\n");
}



void CLI::processRxData(uint8_t byte) {
   // uint8_t byte;

    // Handle transparent mode (shouldn't reach here, but just in case)
    if (destHuart_ != nullptr) {
        HAL_UART_Transmit(destHuart_, &byte, 1, 100);
        return;
    }

}







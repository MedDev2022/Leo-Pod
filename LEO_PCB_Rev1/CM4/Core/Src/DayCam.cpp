/****************************************************************************
**  Name:    DayCam Device Driver (inherits UartDevice)                      **
**  Author:  MK Medical Device Solutions Ltd.                             **
**  Website: www.mkmeddev.com                                             **
****************************************************************************/
#include "DayCam.hpp"
#include <cstdio>
#include <iostream>
#include <cstring>
#include <queue>
//#include "FreeRTOS.h"
//#include "task.h"
#include "cmsis_os.h"




DayCam::DayCam(UART_HandleTypeDef* huart)
    : UartEndpoint(huart) {}

void DayCam::Init() {
//    static uint8_t byte;

	StartReceive(&byte_, 1);
	setProtocol();

	osDelay(1000);

	setAddress();

	osDelay(1000);

	ifClear();

	osDelay(10000);


    if (!StartReceive(&byte_, 1)) {
        printf("Day Cam StartReceive failed\r\n");
    }
    else printf("Day Cam StartReceive success\r\n");
}



void DayCam::onReceiveByte(uint8_t byte) {

	std::queue<uint8_t> tempQueue{std::deque<uint8_t>(rxQueue_)};

 //   printf("📥 Received byte: 0x%02X\n", byte);
//    rxBuffer_[rxLength_] = byte_;

	if (rxQueue_.back() == 0xff){
	    printf("rxQueue_: ");
	    while (!rxQueue_.empty()) {
	        printf("0x%02X ", rxQueue_.front());
	        rxQueue_.pop_front();
	    }
	    printf("\n");
	}


    StartReceive(&byte_, 1);  // Re-arm

}

void DayCam::processIncoming() {
    while (!rxQueue_.empty()) {
        uint8_t byte = rxQueue_.front();
        rxQueue_.pop_front();
//        message_.push_back(byte);

        // Example: parse line-terminated message
        if (byte == '\n') {
            printf("Client received: ");
            for (uint8_t c : rxQueue_)
                printf("%c", c);
            printf("\r\n");

            rxQueue_.clear();  // ready for next message
        }
    }
}

void DayCam::setProtocol() {
//    static uint8_t byte;

	const uint8_t command[] = {0x30, 0x01, 0x00};

	this->SendCommand(command, 3);


}

void DayCam::ifClear() {
//    static uint8_t byte;

	const uint8_t command[] = {0x88, 0x01, 0x00, 0x01, 0xFF};

	this->SendCommand(command, 5);


}

void DayCam::setAddress() {
//    for (int i = 0; i < 4; i++) {
//    	SendCommand(&address_command[i],4);
//    }

	SendCommand(address_command,4);
}

void DayCam::handleZoomIn(uint8_t* speed, uint8_t length){

	uint8_t temp_buff[sizeof(zoom_teleVar)] = {0};

	memcpy(temp_buff,zoom_teleVar,sizeof(zoom_teleVar));

	if (speed[0] > 0 && speed[0] < 8)
		temp_buff [4] = ((temp_buff [4] & 0xF0) | (speed[0] & 0x0F));
	else
		return;

	SendCommand(temp_buff, sizeof(temp_buff));

}

void DayCam::handleZoomOut(uint8_t* speed, uint8_t length){

	uint8_t temp_buff[sizeof(zoom_teleVar)] = {0};

	memcpy(temp_buff,zoom_wideVar,sizeof(zoom_teleVar));

	if (speed[0] > 0 && speed[0] < 8)
		temp_buff [4] = ((temp_buff [4] & 0xF0) | (speed[0] & 0x0f));
	else
		return;

	SendCommand(temp_buff, sizeof(temp_buff));

}

void DayCam::handleZoom2Position(uint16_t position){

	uint8_t temp_buff[sizeof(zoom2Position)] = {0};

	memcpy(temp_buff,zoom2Position,sizeof(zoom2Position));

	if (position <= 0x4000){
		temp_buff[4] = (position & 0xF000) >> 12;
		temp_buff[5] = (position & 0x0F00) >> 8;
		temp_buff[6] = (position & 0x00F0) >> 4;
		temp_buff[7] = (position & 0x000F) ;
	}
	else
		return;
	SendCommand(temp_buff, sizeof(temp_buff));

}

void DayCam::handleZoomStop(){
	SendCommand(zoom_stop, sizeof(zoom_stop));

}

void DayCam::handleFocusFar(uint8_t* speed, uint8_t length){

	uint8_t temp_buff[sizeof(focus_farVar)] = {0};

	memcpy(temp_buff,focus_farVar,sizeof(focus_farVar));

	if (speed[0] > 0 && speed[0] < 8)
		temp_buff [4] = ((temp_buff [4] & 0xF0) | (speed[0] & 0x0f));
	else
		return;

	SendCommand(temp_buff, sizeof(temp_buff));

}

void DayCam::handleFocusNear(uint8_t* speed, uint8_t length){

	uint8_t temp_buff[sizeof(focus_nearVar)] = {0};

	memcpy(temp_buff,focus_nearVar,sizeof(focus_nearVar));

	if (speed[0] > 0 && speed[0] < 8)
		temp_buff [4] = ((temp_buff [4] & 0xF0) | (speed[0] & 0x0f));
	else
		return;

	SendCommand(temp_buff, sizeof(temp_buff));

}

void DayCam::handleFocus2Position(uint16_t position){

	uint8_t temp_buff[sizeof(focus2Position)] = {0};

	memcpy(temp_buff,focus2Position,sizeof(focus2Position));

	if (position <= 0x4000){
		temp_buff[4] = (position & 0xF000) >> 12;
		temp_buff[5] = (position & 0x0F00) >> 8;
		temp_buff[6] = (position & 0x00F0) >> 4;
		temp_buff[7] = (position & 0x000F) ;
	}
	else
		return;
	SendCommand(temp_buff, sizeof(temp_buff));

}

void DayCam::handleFocusStop(){
	SendCommand(focus_stop, sizeof(focus_stop));

}


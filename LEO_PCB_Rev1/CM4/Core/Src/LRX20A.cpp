/****************************************************************************
**  Name:    LRX200 Device Driver (inherits UartDevice)                      **
**  Author:  MK Medical Device Solutions Ltd.                             **
**  Website: www.mkmeddev.com                                             **
****************************************************************************/
#include "LRX20A.hpp"
#include <cstdio>
#include <iostream>
#include <cstring>
#include <queue>
#include "cmsis_os.h"
#include "host.hpp"



#define READ_PORT

extern UART_HandleTypeDef* g_DebugUart;

LRX20A::LRX20A(UART_HandleTypeDef* huart, uint32_t baudrate)
: UartEndpoint(huart, "LRX20Task") {
	baudrate_ = baudrate;
}


void LRX20A::Init() {

    // Set baudrate before starting
    if (huart_->Init.BaudRate != baudrate_) {
        SetBaudrate(baudrate_);
    }

    if (!StartReceive()) {
        printf("LRF Start Receive failed\r\n");
    }
    else printf("LRF Start Receive success\r\n");
}

void LRX20A::InitLRX20A()
{
	rangeData.range1 = 1;
	rangeData.range2 = 4;
	rangeData.range3 = 7;

	distanceResponseStatus.ERR = 	0x00;
	distanceResponseStatus.LA = 	0x00;
	distanceResponseStatus.LPW = 	0x00;
	distanceResponseStatus.MT = 	0x00;
	distanceResponseStatus.NR = 	0x00;
	distanceResponseStatus.NT = 	0x00;
	distanceResponseStatus.TTE = 	0x00;

	SetMinimumRangeCommand();
	HAL_Delay(100);
	SetMaximumRangeCommand();

    StartReceive();  // Re-arm

}


//
//	// ============================================================================
//	// PROCESS DATA RECEIVED FROM LASER RANGE FINDER CONTROLLER
//	// Encrypt and send back to Host for forwarding to external controller
//	// ============================================================================
//size_t LRX20A::processRxData(const uint8_t* data, size_t length) {
//	    if (data == nullptr || length == 0) {
//	        return 0;
//	    }
//
//		#ifdef DEBUG_LRF
//			printf("LRF RX: %u bytes: ", length);
//			for (size_t i = 0; i < length; i++) {
//				printf("%02X ", data[i]);
//			}
//			printf("\r\n");
//		#endif
//
//
//	    // Normal mode - send plain data to Host (Host will encrypt)
//	    if (destEndpointW_ != nullptr) {
//	        printf("LRF: Sending response to Host\r\n");
//	        Host* host = static_cast<Host*>(destEndpointW_);
//	        host->sendDeviceResponse(LRF_ID, data, length);  // Plain data - Host encrypts
//	    } else {
//	        printf("LRF: No Host endpoint configured\r\n");
//	    }
//	}

static constexpr uint8_t LRF_SYNC = 0x59;

size_t LRX20A::processRxData(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return 0;
    }

    // Find sync byte
    if (data[0] != LRF_SYNC) {
        return 1;  // Skip to resync
    }

    // Need at least sync + cmd
    if (length < 2) {
        return 0;
    }

    uint8_t cmd = data[1];
    size_t frameSize = 0;

    switch (cmd) {
    case STATUS_CMD:
        frameSize = 2 + 4;  // sync + cmd + 4 status bytes
        break;
    case EXEC_RANGE_MEASURE:
        frameSize = 2 + 20;  // sync + cmd + 20 range bytes
        break;
    default:
        printf("LRF: Unknown cmd 0x%02X, skipping\r\n", cmd);
        return 2;  // Skip sync + unknown cmd
    }

    // Wait for complete frame
    if (length < frameSize) {
        return 0;
    }

#ifdef DEBUG_LRF
    printf("LRF RX: %u bytes: ", (unsigned)frameSize);
    for (size_t i = 0; i < frameSize; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");
#endif

    // Parse locally
    const uint8_t* payload = &data[2];
    switch (cmd) {
    case STATUS_CMD:
        parseStatus(payload);
        break;
    case EXEC_RANGE_MEASURE:
        parseRanges(payload);
        break;
    }

    // Forward to Host
    if (destEndpointW_ != nullptr) {
        Host* host = static_cast<Host*>(destEndpointW_);
        host->sendDeviceResponse(LRF_ID, data, frameSize);
    }

    return frameSize;
}

void LRX20A::parseStatus(const uint8_t* payload) {
    uint8_t byte3 = payload[2];
    distanceResponseStatus.MT  = byte3 & 0x02;
    distanceResponseStatus.NT  = byte3 & 0x04;
    distanceResponseStatus.ERR = byte3 & 0x08;
    distanceResponseStatus.NR  = byte3 & 0x10;
    distanceResponseStatus.TTE = byte3 & 0x20;
    eDistanceResponseStatus = EDistanceResponseStatus::eUpdated;
}

void LRX20A::parseRanges(const uint8_t* payload) {
    memcpy(&rangeData.range1, &payload[0],  sizeof(float));
    memcpy(&rangeData.range2, &payload[6],  sizeof(float));
    memcpy(&rangeData.range3, &payload[12], sizeof(float));
    eRangesState = ERangesState::eRangesUpdated;
}

void LRX20A::HandleResponseEvent()
{
    uint8_t syncByte;

    if (READ_PORT(&syncByte, 1) == 0) {
        return;
    }

    if (syncByte != 0x59) {
        return;
    }

    uint8_t cmd;
    if (READ_PORT(&cmd, 1) != 0) {
        HandleValidResponse(cmd);
    }
}



void LRX20A::HandleValidResponse(char cmd)
{
    switch (cmd)
    {
    case STATUS_CMD:
    {
        uint8_t buffer[4];
        uint32_t bytesRead = READ_PORT(buffer, sizeof(buffer));

        if (bytesRead > 0) {
            uint8_t byte3 = buffer[2];
            distanceResponseStatus.MT  = byte3 & 0x02;
            distanceResponseStatus.NT  = byte3 & 0x04;
            distanceResponseStatus.ERR = byte3 & 0x08;
            distanceResponseStatus.NR  = byte3 & 0x10;
            distanceResponseStatus.TTE = byte3 & 0x20;

            eDistanceResponseStatus = EDistanceResponseStatus::eUpdated;
        }
        break;
    }
    case EXEC_RANGE_MEASURE:
        RangesDataResponse();
        break;

    default:
    	printf("LRF: Unknown command: 0x%02X\r\n", cmd);
        break;
    }
}


//============================================================================================
//  Minimum Range Command
void LRX20A::SetMinimumRangeCommand()
{
	// Send "Get distance" command
	const char cmd[] = { (char)SET_MIN_RANGE_CMD, (char)MIN_RANGE, (char)0x00, (char)0x00 };
	UpdateCRC((char*)&cmd, (uint8_t)sizeof(cmd));
	SendCommand((uint8_t*)&cmd, sizeof(cmd));

}
//============================================================================================
//  Maximum Range Command
void LRX20A::SetMaximumRangeCommand()
{
	// Send "Get distance" command
	const char cmd[] = { (char)SET_MAX_RANGE_CMD, (char)MAX_RANGE_LSB, (char)MAX_RANGE_MSB, (char)0x00 };
	UpdateCRC((char*)&cmd, (uint8_t)sizeof(cmd));
	SendCommand((uint8_t*)&cmd, sizeof(cmd));

}
//============================================================================================
//  Status Command
bool LRX20A::StatusCommand()
{
	eDistanceResponseStatus = EDistanceResponseStatus::eUpdating;
	// Send "Get distance" command
	const uint8_t getDistanceCmd[] = { (char)STATUS_CMD, (char)0x97 };
	SendCommand(getDistanceCmd, sizeof(getDistanceCmd));
	return 0;
}
//------------------------------------------------------------------------------
//================================================================================
// Ranges Command
bool LRX20A::RangesDataCommand()
{
	eRangesState = ERangesState::eRangesUpdating;
	const uint8_t getDistanceCmd[] = { (char)EXEC_RANGE_MEASURE, (char)MEASURE_MODE_SSM, (char)BIT_RESERVED, (char)BIT_RESERVED, (char)0x9C };
	SendCommand(getDistanceCmd, sizeof(getDistanceCmd));
	return 0;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool LRX20A::RangesDataResponse()
{
    uint8_t buffer[20];

    if (READ_PORT((uint8_t*)&buffer, sizeof(buffer)) != 20) {
        return false;
    }

    // Parse range data from buffer
    // Bytes 0-3:   Range 1 (float, little-endian)
    // Bytes 6-9:   Range 2 (float, little-endian)
    // Bytes 12-15: Range 3 (float, little-endian)

    memcpy(&rangeData.range1, &buffer[0], sizeof(float));
    memcpy(&rangeData.range2, &buffer[6], sizeof(float));
    memcpy(&rangeData.range3, &buffer[12], sizeof(float));

    eRangesState = ERangesState::eRangesUpdated;

    return true;
}

//------------------------------------------------------------------------------
void LRX20A::UpdateCRC(char* pbyBuff, uint8_t size)
{
	long ii;
	char byXorSum;
	byXorSum = 0;
	for (ii = 0; ii < size - 1; ++ii) {
		byXorSum = byXorSum ^ pbyBuff[ii];
	}
	pbyBuff[size - 1] = byXorSum;
}








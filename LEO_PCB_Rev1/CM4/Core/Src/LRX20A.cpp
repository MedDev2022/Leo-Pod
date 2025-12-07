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


void LRX20A::processRxData(uint8_t byte) {
   // uint8_t byte;

    // Handle transparent mode (shouldn't reach here, but just in case)
    if (destEndpoint_ != nullptr) {
        destEndpoint_->write(&byte, 1);
    }

}

void LRX20A::HandleResponseEvent()
{
	uint32_t dwRead;
	char  chRead;
	char  chReadCmd;

	if (READ_PORT((uint8_t*)&chRead, 1) != 0)
	{

		if (chRead == 0x59)
		{
			if (READ_PORT((uint8_t*)&chReadCmd, 1) != 0)
			{
				HandleValidResponse(chReadCmd);
			}
		}
	}
}

void LRX20A::HandleValidResponse(char cmd)
{

	uint32_t dwRead;
	char* outstrCmd, * outstrSyncByte;
	char* outstr;

	outstrSyncByte = "59"; // to_hex_string((uint8_t *)&SYNC_BYTE, sizeof(SYNC_BYTE));
//	outstrCmd = Utils::to_hex_string((uint8_t*)&cmd, sizeof(char));
//	string cmdStr = "Command = ";
//	cmdStr = cmdStr + string(outstrCmd);
//	Utils::DebugMsg("LRX20A HandleResponseEvent:   Arduino <========== Laser   - Verified as CMD ");
//	Utils::DebugMsg((char *)cmdStr.c_str());
	switch (cmd)
	{
	case (char)STATUS_CMD:
	{
		// cout << endl << "LRX20A HandleValidResponse:  CMD=" << cmd << " Arduino <==========================  LASER" << endl;
		char buffer[4];
		dwRead = READ_PORT((uint8_t*)&buffer, sizeof(buffer));
		if (dwRead > 0)
		{

//			outstr = Utils::to_hex_string((uint8_t*)&buffer, sizeof(buffer));

			free(outstr);

			uint8_t byte3 = buffer[2];
			distanceResponseStatus.MT = 	byte3 & 0x02;
			distanceResponseStatus.NT = 	byte3 & 0x04;
			distanceResponseStatus.ERR = 	byte3 & 0x08;
			distanceResponseStatus.NR = 	byte3 & 0x10;
			distanceResponseStatus.TTE = 	byte3 & 0x20;

			//distanceResponseStatusUpdated = true;
			eDistanceResponseStatus = EDistanceResponseStatus::eUpdated;

		}
		break;
	}
	case (char)EXEC_RANGE_MEASURE:
	{
		RangesDataResponse();
		return;
		break;
	}
	default:
		// cout << endl << "UnKnown command: " << outstrCmd << endl;
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


	uint32_t dwRead;
	char* outstrCmd, * outstrSyncByte;
	char* outstr;

	outstrSyncByte = "59";
	outstrCmd = "cc";

	char buffer[20];
	if (READ_PORT((uint8_t*)&buffer, sizeof(buffer)) == 20)
	{
		////================================================================================================================================================
		////  Two targets
		////  a79ab6440600274db24403000000012000004064
		////  Ranges:  Range1 = 1460.832886  Range2 = 1426.411011  Range3 = 0.000000
		////================================================================================================================================================
		//char ranges[] = { (char)0xa7, (char)0x9a, (char)0xb6, (char)0x44, (char)0x06, (char)0x00, (char)0x27, (char)0x4d, (char)0xb2, (char)0x44,
		//	(char)0x03, (char)0x00, (char)0x00, (char)0x00, (char)0x01, (char)0x20, (char)0x00, (char)0x00, (char)0x40, (char)0x64 };
		//memcpy(buffer, ranges, 20 * sizeof(char));
		////================================================================================================================================================


//		outstr = Utils::to_hex_string((uint8_t*)&buffer, sizeof(buffer));
		free(outstr);

//		float range1 = Utils::bytes2Float(buffer[3], buffer[2], buffer[1], buffer[0]);
	//	rangeData.range1 = range1;

//		float range2 = Utils::bytes2Float(buffer[9], buffer[8], buffer[7], buffer[6]);
	//	rangeData.range2 = range2;

	//	float range3 = Utils::bytes2Float(buffer[15], buffer[14], buffer[13], buffer[12]);
	//	rangeData.range3 = range3;

		eRangesState = ERangesState::eRangesUpdated;

	}
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








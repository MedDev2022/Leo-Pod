#ifndef LRX20A_H_
#define LRX20A_H_

#include "UartDevice.h"

#define serial_LSR serial_1

#define SYNC_BYTE              0x59  //  synchronization byte
	//====================================================================================
#define EXEC_RANGE_MEASURE     0xCC
#define BIT_RESERVED           0x00
// MEASURE_MODES:
#define MEASURE_MODE_SSM       0x00  // Single Measurement Mode
#define MEASURE_MODE_QSMM1     0x10  // Quick Single Measurement - Reduced measuring time
#define MEASURE_MODE_QSMM2     0x20  // Quick Single Measurement - Lower range performance
// Continuous Measurement Mode:
#define MEASURE_MODE_CMM1      0x01  //   1 Hz
#define MEASURE_MODE_CMM4      0x02  //   4 Hz
#define MEASURE_MODE_CMM10     0x03  //  10 Hz
#define MEASURE_MODE_CMM20     0x04  //  20 Hz
#define MEASURE_MODE_CMM100    0x05  // 100 Hz
#define MEASURE_MODE_CMM200    0x06  // 200 Hz
//==============================================================================
#define SET_MIN_RANGE_CMD      0x31  // Set minimum range
#define MIN_RANGE              0x19  // LRX-20A min range = 25m
//==============================================================================
#define SET_MAX_RANGE_CMD      0x32  // Set maximum range
#define MAX_RANGE_LSB          0x64  // LRX-20A max range 32100 (7D64)
#define MAX_RANGE_MSB          0x7D
//==============================================================================
#define STATUS_CMD             0xC7  // Ask Status

//==============================================================================
#define ASK_RANGE_WINDOW_CMD             0x30  // Ask range window
#define ASK_RANGE_WINDOW_CHECK_BYTE      0x60  // Check byte


// Mock
//#define TEST_MOCK_READ___x
#define MOCK_READ_STATUS_FULL_DATA  1
#define MOCK_READ_STATUS_DATA       2
#define MOCK_READ_RANGES_FULL_DATA  3
#define MOCK_READ_RANGES_DATA       4

#include "utils/Utils.h"
#include <stdio.h>
#include <iostream>
using namespace std;

typedef struct {
	uint8_t LPW : 1;
	uint8_t LA : 1;
	uint8_t TTE : 1;  // Transmitter timing error. (Contact service)
	uint8_t NR : 1;   // Not Ready to perform. Too high requested. Wait until the bit is cleared.
	uint8_t ERR : 1;  // Error reported by LRX
	uint8_t NT : 1;   // No Targets
	uint8_t MT : 1;
	uint8_t : 1;
} DistanceResponseStatus_t;

typedef struct {
	uint8_t sync;
	uint8_t echo;
	float range1;   // [meter]
	uint16_t signalLevel1;
	float range2;   // [meter]
	uint16_t signalLevel2;
	float range3;   // [meter]
	uint16_t signalLevel3;
	DistanceResponseStatus_t status;
	uint8_t check; // checksum XOR 50h
} DistanceResponse_t;

typedef struct {
	float range1;   // [meter]
	uint16_t signalLevel1;
	float range2;   // [meter]
	uint16_t signalLevel2;
	float range3;   // [meter]
	uint16_t signalLevel3;
	DistanceResponseStatus_t status;
	uint8_t check; // checksum XOR 50h
} RangesData_t;



typedef enum {
	eNotUpdated = 0,
	eUpdating,
	eUpdated
} EDistanceResponseStatus;

typedef enum {
	eRangesNotUpdated = 0,
	eRangesUpdating,
	eRangesUpdated
} ERangesState;


class LRX20A : public UartDevice {
public:

	LRX20A(USART_TypeDef * portName , uint32_t defaultBaudRate = 115200, uint32_t defaultStopBit = UART_STOPBITS_1);

	LRX20A();
	void InitLRX20A();
	virtual ~LRX20A();
	
	void HandleResponseEvent();
	//-----------------------------------------------------------------
	bool StatusCommand();
	//-----------------------------------------------------------------
	bool RangesDataCommand();
	bool RangesDataResponse();
	void UpdateCRC(char* pbyBuff, uint8_t size);
	long laser_read_port(uint8_t* buffer, uint8_t size);
	int laser_write_port(uint8_t* buffer, uint8_t size);
    void HandleReceivedData(const std::vector<uint8_t>& data) override;  // Implement this function

	EDistanceResponseStatus eDistanceResponseStatus = EDistanceResponseStatus::eNotUpdated;
	DistanceResponseStatus_t distanceResponseStatus;

	ERangesState eRangesState = ERangesState::eRangesNotUpdated;
	RangesData_t rangeData;

private:
	
	void SetMinimumRangeCommand();
	void SetMaximumRangeCommand();
	void HandleValidResponse(char cmd);
};

#endif /* LRX20A_H_ */

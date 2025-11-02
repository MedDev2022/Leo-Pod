#ifndef HOST_HPP
#define HOST_HPP

#pragma once

#include "UartEndpoint.hpp"
#include <deque>
#include "comm.hpp"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "timers.h"  // FreeRTOS software timers


class DayCam;
class LRX20A;
class IRay;
class RPLens;
class CLI;

struct RxMessage {
    uint8_t* data;
    size_t length;
};


typedef union {
    uint16_t value;   // packed 32-bit register/value
    struct {
        unsigned COOLING_ON        : 1;  // bit 0
        unsigned HEATER_SDI_CONV   : 1;  // bit 1
        unsigned HEATER_ON_IRCAM   : 1;  // bit 2
        unsigned HEATER_MISC       : 1;  // bit 3
        unsigned DAYCAM_TEC_ON     : 1;  // bit 4
        unsigned DAYCAM_COOL_HEAT  : 1;  // bit 5 (0=cool,1=heat) if that's your meaning
        unsigned IR_TEC_ON         : 1;  // bit 6
        unsigned IR_COOL_HEAT      : 1;  // bit 7
        unsigned RP_TEC_ON         : 1;  // bit 8
        unsigned RP_COOL_HEAT      : 1;  // bit 9
        unsigned _reserved         : 6; // bits 10..15
    } bits;
} ctrlReg_t;

class Host : public UartEndpoint {
public:
    explicit Host(UART_HandleTypeDef* huart);
    void Init();

    void setDayCam(DayCam* cam);
    void setLRF(LRX20A* lrf);
    void setIRay(IRay* cam);
    void setRPLens(RPLens* lens);
    void setCli(CLI* cli);

	ctrlReg_t tmpReg = {0};

    uint16_t Temp[4] = {0};

    uint16_t Imu[3] = {0};

protected:
    void processRxData(uint8_t byte) override;



private:

    void parseAndProcess(const comm::Message& msg);
    void resetReception();
    bool verifyCRC(uint8_t* msg, size_t len);

    // Timer management
    void startTimeoutTimer();
    void stopTimeoutTimer();
    void handleTimeout();
    static void timeoutTimerCallback(TimerHandle_t xTimer);

    enum class RxState {
        Ready,
        Receiving,
		Idle,
		Stopped
    };

    RxState rxState_ = RxState::Ready;
    uint8_t buffer_[256];
    size_t bufferIndex_ = 0;
    size_t expectedLength_ = 0;
    TickType_t firstByteTick_ = 0;

    // FreeRTOS software timer for receive timeout
    TimerHandle_t timeoutTimer_ = nullptr;
    static constexpr uint32_t TIMEOUT_MS = 100;



    comm::Message rxMsg;
    comm::Message txMsg;

    // Device pointers
    DayCam* dayCam_ = nullptr;
    LRX20A* lrx20A_ = nullptr;
    IRay* iRay_ = nullptr;
    RPLens* rpLens_ = nullptr;
    CLI* cli_ = nullptr;
};

#endif // HOST_HPP

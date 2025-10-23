#pragma once
#include "UartEndpoint.hpp"
#include <deque>
#include "comm.hpp"
#include "FreeRTOS.h"
#include "cmsis_os2.h"


class DayCam;
class LRX20A;
class IRay;
class RPLens;

struct RxMessage {
    uint8_t* data;
    size_t length;
};

enum class RxState {
    Idle,
	Ready,
    Receiving,
    Stopped
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

class Client : public UartEndpoint {
public:
    explicit Client(UART_HandleTypeDef* huart);
    void Init();
    void onReceiveByte(uint8_t byte) override;
    void setDayCam(DayCam* cam);
    void setLRF(LRX20A* lrf);
    void setIRay(IRay* cam);
    void setRPLens(RPLens* lens);
    void resetReception();
    bool receiving_ = false;
    void processIncomingThread(); // new task entrypoint

	ctrlReg_t tmpReg = {0};

private:
    uint8_t byte_;
    std::deque<uint8_t> message_;  // buffer for current message
    comm::Message rxMsg;

    osMessageQueueId_t rxMsgQueue_{nullptr};  // Queue of RxMessage

    std::deque<comm::Message> rxMsgQuee_;

    uint8_t buffer_[256];
    size_t bufferIndex_ = 0;
    TickType_t firstByteTick_ = 0;

    size_t expectedLength_ = 0;
    DayCam* dayCam_ = nullptr;
    LRX20A* lrx20A_ = nullptr;
    RxState rxState_ = RxState::Idle;
    IRay* iRay_ = nullptr;
    RPLens* rpLens_ = nullptr;

protected:

    void parseAndProcess(uint8_t* msg, size_t len);
    void processIncoming() override;
    bool verifyCRC(uint8_t* msg, size_t len);


};




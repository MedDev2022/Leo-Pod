

#ifndef SRC_RPLENS_H_
#define SRC_RPLENS_H_

#pragma once
#include "UartEndpoint.hpp"
#include <queue>
#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include "main.h"



class RPLens : public UartEndpoint {
public:

    explicit RPLens(UART_HandleTypeDef* huart);
//	RPLens(USART_TypeDef * portName, int defaultBaudRate = 9600);
    void Init();

    // Connection handling
 //   bool Connect(USART_TypeDef * portName, int baudRate = 115200) ;

    // Device commands
    void ZoomStop();
    void ZoomIn();
    void ZoomOut();
    void SetZoomPosition(int pos);
    void SetFocusNear();
    void SetFocusFar();
    void SetContinuousFocusFar();
    void SetFocusStop();
    void SetContinuousFocusNear();
    void SetFocusIncrement(int pos);
    void SetFocusDecrement(int pos);
    void FocusStop();
    void SetZoomAndFocusPosition(int zoomPos, int focusPos);
    void SetFastFocusPosition(int focusPos);

    void handleZoomIn(void);
    void handleZoomOut(void);
    void handleZoomStop(void);
    // Command processing
    void ProcessData();

    // Parse responses for zoom and focus positions
    std::map<std::string, int> ParseResponse(const std::vector<uint8_t>& command);

protected:
//    void onReceiveByte(uint8_t byte) override;
//    void processIncoming() override;
    void processRxData(uint8_t byte) override;
private:


    std::deque<uint8_t> messageBuffer_;
    uint8_t byte_;

    struct InputMessageInfo {
        uint8_t sync = 0;
        uint8_t commandID = 0;
        uint8_t dataLengthLSB = 0;
        uint8_t dataLengthMSB = 0;
        std::vector<uint8_t> messageBytes;
        int messagePointer = -1;
        int dataLength = 0;

        void BuildMessageContainer();
        std::vector<uint8_t> GetMessage();
        void Reset();
    };

    uint8_t rxBuffer[64];  // Adjust size as necessary for your data

    std::queue<std::vector<uint8_t>> inputDataQueue;
    InputMessageInfo inputMessageInfo;

//    void SendCommand(const std::vector<uint8_t>& command);
    void UpdateCRC(std::vector<uint8_t>& cmd);
};


#endif /* SRC_RPLENS_H_ */

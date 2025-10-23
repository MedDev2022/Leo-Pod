

#ifndef SRC_RPLENS_H_
#define SRC_RPLENS_H_

#include "UartDevice.h"
#include <queue>
#include <map>
#include <string>
#include <vector>
#include <cstdint>



class RPLens : public UartDevice {
public:


	RPLens(USART_TypeDef * portName, int defaultBaudRate = 9600);


    // Connection handling
    bool Connect(USART_TypeDef * portName, int baudRate = 115200) override;

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


    // Command processing
    void ProcessData();

    // Parse responses for zoom and focus positions
    std::map<std::string, int> ParseResponse(const std::vector<uint8_t>& command);

    void HandleReceivedData(const std::vector<uint8_t>& data) override;  // Implement this function
   // void onDataReceivedHandler(const std::vector<uint8_t>& command) override;

protected:

  //  void OnDataReceivedHandler(const std::vector<uint8_t>& data) override;

private:
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

static RPLens rpLens(USART2, 19200);

#endif /* SRC_RPLENS_H_ */

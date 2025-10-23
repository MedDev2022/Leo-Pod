
#ifndef SRC_UARTDEVICE_H_
#define SRC_UARTDEVICE_H_

#include "main.h"
#include <functional>
#include <string>
#include <vector>
#include <iostream>
#include "uart.h"


class UartDevice {
public:
    UartDevice(USART_TypeDef * portName, uint32_t baudRate,
               uint32_t parity, uint32_t stopBits);

    UartDevice(UART_HandleTypeDef* huart, uint32_t defaultBaudRate = 9600, uint32_t defaultStopBit = UART_STOPBITS_1);

    virtual ~UartDevice();

    // Public properties

    // Callbacks
    std::function<void(const std::vector<uint8_t>&)> OnDataReceived;

    // Device command methods
//    virtual void StartTechnicianModeCMD() = 0;
//    virtual void StartTransparentModeCMD() = 0;
//    virtual void StartBootloaderModeCMD() = 0;
//    virtual void StartWorkingModeCMD() = 0;

    virtual bool Connect(USART_TypeDef * portName, int baudRate);
    bool Connect(USART_TypeDef * portName, int baudRate, uint32_t parity, uint32_t stopBits);

    bool Disconnect();

    virtual void HandleReceivedData(const std::vector<uint8_t>& data) = 0;

    void ProcessReceivedData(const std::vector<uint8_t>& data);  // Public wrapper
    //void StartReceive(uint8_t* rxBuffer, size_t length);
    void StartReceive(void* rxBuffer, size_t length);

    void println(const char* message);

protected:
    USART_TypeDef * DefaultPortName = USART1;
    uint32_t DefaultBaudRate = 115200;
    uint32_t DefaultParity = UART_PARITY_NONE;
    uint32_t DefaultStopBits = UART_STOPBITS_1;
    bool HardwareConnected = false;
    void SendCommand(const uint8_t* command, size_t length);


    USART_TypeDef * GetPort();



    static void UART_RxCpltCallback(UART_HandleTypeDef* huart);



private:
    void OnDataReceivedHandler(const std::vector<uint8_t>& data);
    // UART initialization helper
    bool InitializeUART(int baudRate, uint32_t parity, uint32_t stopBits);
    UART port;

};

#endif /* SRC_UARTDEVICE_H*/

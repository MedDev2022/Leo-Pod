
#include "UartDevice.h"
#include <sstream>
#include <iomanip>
#include "uart.h"
#include <map>



UartDevice::UartDevice(USART_TypeDef * portName, uint32_t dwBaudRate,
                       uint32_t blParity, uint32_t stopBits)
    :  DefaultPortName(portName), DefaultBaudRate(dwBaudRate),
      DefaultParity(blParity), DefaultStopBits(stopBits)
{
	//InitializeUART(dwBaudRate, blParity, stopBits);
	port.init(portName, dwBaudRate, blParity);
	HardwareConnected = true;
}


UartDevice::~UartDevice() {
    Disconnect();
}

bool UartDevice::InitializeUART(int baudRate, uint32_t parity, uint32_t stopBits) {


	port.huart->Init.BaudRate = baudRate;
	port.huart->Init.WordLength = UART_WORDLENGTH_8B;
	port.huart->Init.StopBits = stopBits;
	port.huart->Init.Parity = parity;
	port.huart->Init.Mode = UART_MODE_TX_RX;
	port.huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
	port.huart->Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(port.huart) != HAL_OK) {
        return false;
    }
    HardwareConnected = true;
    return true;
}

bool UartDevice::Connect(USART_TypeDef * portName, int baudRate) {
    return Connect(portName, baudRate, DefaultParity, DefaultStopBits);
}

bool UartDevice::Connect(USART_TypeDef * portName, int baudRate, uint32_t parity, uint32_t stopBits) {
    DefaultPortName = portName;
    DefaultBaudRate = baudRate;
    HardwareConnected = true;
    return InitializeUART(baudRate, parity, stopBits);
}

bool UartDevice::Disconnect() {
    if (HardwareConnected) {
        HAL_UART_DeInit(port.huart);
        HardwareConnected = false;
    }
    return !HardwareConnected;
}

void UartDevice::SendCommand(const uint8_t* command, size_t length) {

    port.send(const_cast<uint8_t*>(command), length);

}

void UartDevice::println(const char* message) {
    port.println(message);

}

///void UartDevice::StartReceive(uint8_t * rxBuffer, size_t length) {
void UartDevice::StartReceive(void * rxBuffer, size_t length) {
	port.StartReceive(rxBuffer, length);
}


//void UartDevice::OnDataReceivedHandler(const std::vector<uint8_t>& data) {
//    if (OnDataReceived) {
//        OnDataReceived(data);
//    }
//}
void UartDevice::ProcessReceivedData(const std::vector<uint8_t>& data) {
    OnDataReceivedHandler(data);  // Calls the protected function
}

void UartDevice::OnDataReceivedHandler(const std::vector<uint8_t>& data) {
    HandleReceivedData(data);  // Calls the derived class implementation
}

USART_TypeDef * UartDevice::GetPort(){
return port.getHuart();
}

extern std::map<USART_TypeDef*, UartDevice*> deviceMap; // Declare global map
// Example of an interrupt callback for receiving data
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	 __NOP();

	    if (huart && huart->Instance) {
	        auto it = deviceMap.find(huart->Instance);
	        if (it != deviceMap.end()) {
	            UartDevice* device = it->second;
	            std::vector<uint8_t> data(huart->pRxBuffPtr-huart->RxXferSize, huart->pRxBuffPtr );
	            device->ProcessReceivedData(data);  // Call public wrapper
	            //device->StartReceive(huart->pRxBuffPtr, huart->RxXferSize);  // Restart reception
	            device->StartReceive(huart->pRxBuffPtr-huart->RxXferSize, huart->RxXferSize);  // Restart reception
	        }
	    }
}



#include "Fpga.h"
#include "main.h"
#include "uart.h"
#include "RPLens.h"



Fpga::Fpga(USART_TypeDef * portName, int defaultBaudRate)
    : UartDevice(portName, defaultBaudRate, UART_PARITY_NONE, UART_STOPBITS_1) {


}

bool Fpga::StartReceive() {
     //UartDevice::Connect(portName, baudRate);

	UartDevice::StartReceive(&rxMsg, 132);

	__NOP();


     return true;
}
//
//void IRay::StartTechnicianModeCMD() {
//    uint8_t startMaintenanceMode[] = {0x10, 0x20, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65};
//    SendCommand(startMaintenanceMode, sizeof(startMaintenanceMode));
//}
//
//void IRay::StartTransparentModeCMD() {
//    uint8_t startTransparentMode[] = {0x10, 0x20, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66};
//    SendCommand(startTransparentMode, sizeof(startTransparentMode));
//}
//
//void IRay::StartBootloaderModeCMD() {
//    uint8_t startBootloaderMode[] = {0x10, 0x20, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68};
//    SendCommand(startBootloaderMode, sizeof(startBootloaderMode));
//}
//
//void IRay::StartWorkingModeCMD() {
//    uint8_t startWorkingMode[] = {0x10, 0x20, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67};
//    SendCommand(startWorkingMode, sizeof(startWorkingMode));
//}




void Fpga::HandleReceivedData(const std::vector<uint8_t>& data) {
    // Example logic to process incoming data



    if (data.empty()) {
        std::cout << "Received empty data" << std::endl;
        SendCommand(rxBuffer, 8);
        return;
    }
    SendCommand(rxBuffer, 8);

    std::cout << "FPGA received data: ";
    for (uint8_t byte : data) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;
    //UartDevice::StartReceive(&rxMsg, rxMsg.SIZE_BYTES);
    // Add further parsing or handling logic here
}






#include "IRay.h"

IRay::IRay(USART_TypeDef * portName, int defaultBaudRate)
    : UartDevice(portName, defaultBaudRate, UART_PARITY_NONE, UART_STOPBITS_1) {
    InitializePalettes();
}

bool IRay::Connect(USART_TypeDef * portName, int baudRate) {
    return UartDevice::Connect(portName, baudRate);
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

void IRay::SetPalette(const std::string& palette) {
    if (irPalettes.find(palette) != irPalettes.end()) {
        SendCommand(irPalettes[palette].data(), irPalettes[palette].size());
    }
}

void IRay::SetReticlePosition(int x, int y) {
    uint8_t cmd[] = {0xAA, 0x09, 0x01, 0x44, 0x02, 0x05, 0x64, 0x00, 0x64, 0x00, 0xC7, 0xEB, 0xAA};
    cmd[6] = static_cast<uint8_t>(x & 0xFF);
    cmd[7] = static_cast<uint8_t>((x >> 8) & 0xFF);
    cmd[8] = static_cast<uint8_t>(y & 0xFF);
    cmd[9] = static_cast<uint8_t>((y >> 8) & 0xFF);
    SendCommand(cmd, sizeof(cmd));
}

void IRay::GetReticlePosition() {
    uint8_t cmd[] = {0xAA, 0x04, 0x01, 0x44, 0x00, 0xF3, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::ReticleOn() {
    uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0xc1, 0xb6, 0xeb, 0xaa};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::CalibReticleOFF() {
    uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x40, 0x35, 0xeb, 0xaa};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::ReticleOFF() {
    uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x00, 0xF5, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::Reticle1() {
    uint8_t color[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x80, 0x75, 0xEB, 0xAA};
    SendCommand(color, sizeof(color));
    HAL_Delay(1000);
    uint8_t cmd[] = { 0xAA, 0x05, 0x01, 0x43, 0x02, 0x80, 0x75, 0xEB, 0xAA };
    SendCommand(cmd, sizeof(cmd));
    StartReceive(rxBuffer, 8);

}

void IRay::Reticle2() {
    uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x81, 0x76, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::Reticle3() {
    uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x82, 0x77, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::Reticle4() {
    uint8_t cmd[] = {0xAA, 0x05, 0x01, 0x43, 0x02, 0x83, 0x78, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::ReticleSetPosition() {
    uint8_t cmd[] = {0xAA, 0x09, 0x01, 0x44, 0x02, 0x05, 0x64, 0x00, 0x64, 0x00, 0xC7, 0xEB, 0xAA};
    SendCommand(cmd, sizeof(cmd));
}

void IRay::InitializePalettes() {
    irPalettes["WH"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x00, 0xF4, 0xEB, 0xAA};
    irPalettes["BH"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x01, 0xF5, 0xEB, 0xAA};
    irPalettes["Rainbow"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x02, 0xF6, 0xEB, 0xAA};
    irPalettes["BGR"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x03, 0xF7, 0xEB, 0xAA};
    irPalettes["BRY"] = {0xAA, 0x05, 0x01, 0x42, 0x02, 0x04, 0xF8, 0xEB, 0xAA};
}

//void IRay::OnDataReceivedHandler(const std::vector<uint8_t>& command) {
//    // Process incoming data
//    // This is where you would implement data parsing logic
//}

void IRay::HandleReceivedData(const std::vector<uint8_t>& data) {
    // Example logic to process incoming data
    if (data.empty()) {
        std::cout << "Received empty data" << std::endl;

        SendCommand(rxBuffer, 8);
        return;
    }
    SendCommand(rxBuffer, 8);

    std::cout << "IRay received data: ";
    for (uint8_t byte : data) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Add further parsing or handling logic here
}





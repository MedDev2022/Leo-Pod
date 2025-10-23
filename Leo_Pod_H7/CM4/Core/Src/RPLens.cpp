
#include "RPLens.h"
#include <iostream>


RPLens::RPLens(USART_TypeDef * portName, int defaultBaudRate)
   : UartDevice(portName, defaultBaudRate, UART_PARITY_NONE, UART_STOPBITS_1) {}



bool RPLens::Connect(USART_TypeDef * portName, int baudRate) {
    bool isConnected = UartDevice::Connect(portName, baudRate);
    if (isConnected) {
        ProcessData();
    }
    return isConnected;
}

void RPLens::ZoomStop() {
    std::vector<uint8_t> cmd = {0x24, 0x14, 0x00, 0x00, 0x30};
    UpdateCRC(cmd);
    SendCommand(cmd.data(), cmd.size());
}

void RPLens::ZoomIn() {
    std::vector<uint8_t> cmd = {0x24, 0x23, 0x00, 0x00, 0x07};
    UpdateCRC(cmd);
    StartReceive(rxBuffer, 14);  // Start receiving in preparation
    SendCommand(cmd.data(), cmd.size());
}

void RPLens::ZoomOut() {
    std::vector<uint8_t> cmd = {0x24, 0x24, 0x00, 0x00, 0x00};
    UpdateCRC(cmd);
    SendCommand(cmd.data(), cmd.size());
}

void RPLens::SetZoomPosition(int pos) {
    std::vector<uint8_t> cmd = {0x24, 0x47, 0x02, 0x00, static_cast<uint8_t>(pos & 0xFF), static_cast<uint8_t>((pos >> 8) & 0xFF), 0x00};
    UpdateCRC(cmd);
    SendCommand(cmd.data(), cmd.size());
}

//void RPLens::SetZoomAndFocusPosition(int zoomPos, int focusPos) {
//    std::vector<uint8_t> cmd = {0x24, 0x0D, 0x04, 0x00, static_cast<uint8_t>(zoomPos & 0xFF), static_cast<uint8_t>((zoomPos >> 8) & 0xFF),
//                                static_cast<uint8_t>(focusPos & 0xFF), static_cast<uint8_t>((focusPos >> 8) & 0xFF)};
//    UpdateCRC(cmd);
//    SendCommand(cmd.data(), cmd.size());
//}


void RPLens::SetFocusNear()
{
	uint8_t cmd[] = {0x24, 0x0A, 0x0, 0x0, 0x2E };
}

void RPLens::SetContinuousFocusNear()
        {
            // 0x24 0x0A 0x0 0x0 0x2E
	std::vector<uint8_t> cmd = { 0x24, 0x21, 0x0, 0x0, 0x5 };
	UpdateCRC(cmd);
    SendCommand(cmd.data(), cmd.size());
}

void RPLens::SetFocusFar()
{
            // 0x24 0x0A 0x0 0x0 0x2E
	std::vector<uint8_t> cmd = { 0x24, 0x0B, 0x0, 0x0, 0x2F };
	UpdateCRC(cmd);
    SendCommand(cmd.data(), cmd.size());
}

void RPLens::SetContinuousFocusFar()
        {
            // 0x24 0x0A 0x0 0x0 0x2E
	std::vector<uint8_t>  cmd = { 0x24, 0x22, 0x0, 0x0, 0x6 };
	UpdateCRC(cmd);
    SendCommand(cmd.data(), cmd.size());
        }

void RPLens::SetFocusStop()
        {
            // 0x24 0x0A 0x0 0x0 0x2E
	std::vector<uint8_t> cmd = { 0x24, 0x1B, 0x0, 0x0, 0x2F };
	UpdateCRC(cmd);
    SendCommand(cmd.data(), cmd.size());
        }

void RPLens::SetFocusIncrement(int pos)
        {
	std::vector<uint8_t> focusPositionCMD = { 0x24, 0x0A, 0x02, 0x00, 0x0F, 0xFF, 0xFF };
	focusPositionCMD[4] = (pos & 0xFF);
	focusPositionCMD[5] = ((pos & 0xFF00) >> 8);
	UpdateCRC(focusPositionCMD);
    SendCommand(focusPositionCMD.data(), focusPositionCMD.size());
        }

void RPLens::SetFocusDecrement(int pos)
        {
	std::vector<uint8_t> focusPositionCMD = { 0x24, 0x0B, 0x02, 0x00, 0x0F, 0xFF, 0xFF };
	focusPositionCMD[4] = (pos & 0xFF);
	focusPositionCMD[5] = ((pos & 0xFF00) >> 8);
	UpdateCRC(focusPositionCMD);
	SendCommand(focusPositionCMD.data(), focusPositionCMD.size());
        }

void RPLens::FocusStop()
{
	std::vector<uint8_t> focusPositionCMD = { 0x24, 0x1B, 0x00, 0x00, 0xFF };
	UpdateCRC(focusPositionCMD);
	SendCommand(focusPositionCMD.data(), focusPositionCMD.size());
 }

void RPLens::SetZoomAndFocusPosition(int zoomPos, int focusPos)
{
	std::vector<uint8_t>  zoomFocusPositionCMD = { 0x24, 0x0D, 0x4, 0x0, 0x28, 0xA, 0xFC, 0x8, 0xFB };
	zoomFocusPositionCMD[4] = (zoomPos & 0XFF);
	zoomFocusPositionCMD[5] = ((zoomPos & 0XFF00) >> 8);
	zoomFocusPositionCMD[6] = (focusPos & 0XFF);
	zoomFocusPositionCMD[7] = ((focusPos & 0XFF00) >> 8);
	UpdateCRC(zoomFocusPositionCMD);
	SendCommand(zoomFocusPositionCMD.data(), zoomFocusPositionCMD.size());
}


void RPLens::SetFastFocusPosition(int focusPos)
        {
		std::vector<uint8_t> fastFocusPositionCMD = { 0x24, 0x48, 0x05, 0x00, 0xAA, 0xAA, 0xBB, 0xBB, 0x1E, 0xFF };
            fastFocusPositionCMD[4] = (focusPos & 0XFF);
            fastFocusPositionCMD[5] = ((focusPos & 0XFF00) >> 8);
            fastFocusPositionCMD[6] = (focusPos & 0XFF);
            fastFocusPositionCMD[7] = ((focusPos & 0XFF00) >> 8);
            UpdateCRC(fastFocusPositionCMD);
            SendCommand(fastFocusPositionCMD.data(), fastFocusPositionCMD.size());
        }

void RPLens::ProcessData() {
    std::vector<uint8_t> messagePart;
    while (HardwareConnected) {
        if (!inputDataQueue.empty()) {
            messagePart = inputDataQueue.front();
            inputDataQueue.pop();

            for (size_t i = 0; i < messagePart.size(); ++i) {
                int messageIndex = inputMessageInfo.messagePointer + 1;
                if (messageIndex < 4) {
                    switch (messageIndex) {
                        case 0: inputMessageInfo.sync = messagePart[i]; break;
                        case 1: inputMessageInfo.commandID = messagePart[i]; break;
                        case 2: inputMessageInfo.dataLengthLSB = messagePart[i]; break;
                        case 3: inputMessageInfo.dataLengthMSB = messagePart[i]; break;
                    }
                    inputMessageInfo.messagePointer++;
                } else {
                    if (messageIndex < inputMessageInfo.dataLength + 5) {
                        inputMessageInfo.messageBytes[messageIndex] = messagePart[i];
                        inputMessageInfo.messagePointer++;
                    } else {
                        OnDataReceived(inputMessageInfo.GetMessage());
                        inputMessageInfo.Reset();
                    }
                }
            }
        }
    }
}

std::map<std::string, int> RPLens::ParseResponse(const std::vector<uint8_t>& command) {
//    std::map<std::string, int> result;
//    if (command.size() >= 8) {
//        int zoomPos = command[4] + (command[5] << 8);
//        int focusPos = command[6] + (command[7] << 8);
//        result["ZOOM"] = zoomPos;
//        result["FOCUS"] = focusPos;
//    }
	std::map<std::string, int> result;
    int zoomPos;
    int focusPos;
    uint8_t commandID = command[1];
    switch(commandID)
    {
        case 0x29:
            zoomPos = command[4] + (command[5] << 8);
            focusPos = command[6] + (command[7] << 8);
            break;
        case 0x0D:
            //   0x24 0x0D 0x4 0x0 0xAC 0xD 0x28 0xA 0xAE
            zoomPos = command[4] + (command[5] << 8);
            focusPos = command[6] + (command[7] << 8);
            break;

    }

    result["ZOOM"] = zoomPos;
    result["FOCUS"] = focusPos;

    return result;

}

//void RPLens::OnDataReceivedHandler(const std::vector<uint8_t>& command) {
//    inputDataQueue.push(command);
//}

//void RPLens::SendCommand(const std::vector<uint8_t>& command) {
//    HAL_UART_Transmit(huart, const_cast<uint8_t*>(command.data()), command.size(), HAL_MAX_DELAY);
//    std::cout << "Command sent: ";
//    for (uint8_t byte : command) {
//        std::cout << std::hex << static_cast<int>(byte) << " ";
//    }
//    std::cout << std::endl;
//}

void RPLens::UpdateCRC(std::vector<uint8_t>& cmd) {
    uint8_t crc = 0;
    for (size_t i = 0; i < cmd.size() - 1; ++i) {
        crc ^= cmd[i];
    }
    cmd.back() = crc;
}

void RPLens::InputMessageInfo::BuildMessageContainer() {
    dataLength = dataLengthLSB + (dataLengthMSB << 8);
    messageBytes.resize(dataLength + 5);
    messageBytes[0] = sync;
    messageBytes[1] = commandID;
    messageBytes[2] = dataLengthLSB;
    messageBytes[3] = dataLengthMSB;
}

std::vector<uint8_t> RPLens::InputMessageInfo::GetMessage() {
    if (messageBytes.empty() || messageBytes.size() != messagePointer + 1) {
        return {};
    }
    return messageBytes;
}

void RPLens::InputMessageInfo::Reset() {
    sync = 0;
    commandID = 0;
    dataLengthLSB = 0;
    dataLengthMSB = 0;
    messagePointer = -1;
    messageBytes.clear();
}



void RPLens::HandleReceivedData(const std::vector<uint8_t>& data) {
    // Example logic to process incoming data
	//ProcessData();

	ParseResponse(data);

}




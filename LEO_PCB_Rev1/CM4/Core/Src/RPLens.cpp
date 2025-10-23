
#include <RPLens.hpp>
#include <iostream>
#include <cstring>
#include <queue>
#include "cmsis_os.h"


//RPLens::RPLens(USART_TypeDef * portName, int defaultBaudRate)
//   : UartEndpoint(portName, defaultBaudRate, SERIAL_8N1) {}

RPLens::RPLens (UART_HandleTypeDef* huart)
    : UartEndpoint(huart) {}

//bool RPLens::Connect(USART_TypeDef * portName, int baudRate) {
//    bool isConnected = UartEndpoint::Connect(portName, baudRate);
//    if (isConnected) {
//        ProcessData();
//    }
//    return isConnected;
//}

void RPLens::Init()
{


    if (!StartReceive(&byte_, 1)) {
        printf("RPLEns StartReceive failed\n");
    }
    else printf("RPLEns StartReceive success\n");
}

void RPLens::onReceiveByte(uint8_t byte) {

	std::queue<uint8_t> tempQueue{std::deque<uint8_t>(rxQueue_)};

	printf("📥 RP Received byte: 0x%02X\n", byte);
//    rxBuffer_[rxLength_] = byte_;

	if (rxQueue_.back() == 0xff){
	    printf("rxQueue_: ");
	    while (!rxQueue_.empty()) {
	        printf("0x%02X ", rxQueue_.front());
	        rxQueue_.pop_front();
	    }
	    printf("\n");
	}


    StartReceive(&byte_, 1);  // Re-arm

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


void RPLens::handleZoomIn(void){

    uint8_t cmd[] = {0x24, 0x23, 0x00, 0x00, 0x07};
	this->SendCommand(cmd, sizeof(cmd));

}

void RPLens::handleZoomOut(void){

    uint8_t cmd[] = {0x24, 0x24, 0x00, 0x00, 0x00};
	this->SendCommand(cmd, sizeof(cmd));

}

void RPLens::handleZoomStop(void){

    uint8_t cmd[] = {0x24, 0x14, 0x00, 0x00, 0x30};
	this->SendCommand(cmd, sizeof(cmd));

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



void RPLens::processIncoming() {
	while (!rxQueue_.empty()) {
		uint8_t byte = rxQueue_.front();
		rxQueue_.pop_front();
//        message_.push_back(byte);

		// Example: parse line-terminated message
		if (byte == '\n') {
			printf("Client received: ");
			for (uint8_t c : rxQueue_)
				printf("%c", c);
			printf("\r\n");

			rxQueue_.clear();  // ready for next message
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


void onReceiveByte(uint8_t byte){

}
void processIncoming() {

}







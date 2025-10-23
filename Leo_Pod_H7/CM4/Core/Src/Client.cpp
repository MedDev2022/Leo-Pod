
#include "Client.h"
#include "main.h"
#include "uart.h"
#include "RPLens.h"



Client::Client(USART_TypeDef * portName, int defaultBaudRate)
    : UartDevice(portName, defaultBaudRate, UART_PARITY_NONE, UART_STOPBITS_1) {


}

bool Client::StartReceive() {
     //UartDevice::Connect(portName, baudRate);
     UartDevice::StartReceive(&rxMsg, rxMsg.SIZE_BYTES);
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




void Client::HandleReceivedData(const std::vector<uint8_t>& data) {
    // Example logic to process incoming data

	if (data[0]==0x10)__NOP();
	uint8_t byDst, byOpcode, byAdr;
	if (rxMsg.isOK() && rxMsg.getDestID() == MY_ID ){
		byOpcode = rxMsg.getOpCode();
		switch(byOpcode){
		case 0x01: //FOV Zoom in 01h

			Serial1.println("FOV FOV Zoom in");
			println("FOV Zoom in");
			break;
		case 0x02: //FOV Zoom out 02h
			println("FOV Zoom out");
			break;
		case 0x03: //FOV Zoom (position) 03h
			break;
		case 0x04: //FOV Stop Zoom 	04h
			break;
		case 0x05: //FOCUS Focus out (to near)	05h
			break;
		case 0x06: //FOCUS Focus in (to far) 	06h
			break;
		case 0x07: //FOCUS Focus (position)	07h
			break;
		case 0x08: //FOCUS Focus Stop 	08h
			break;
		case 0x09: //AUTOFOCUS Manual /  Auto mode 	09h
			break;
		case 0x0A: //GAIN MODE Manual /  Auto mode 	0Ah
			break;
		case 0x0B: //GAIN MODE Set Gain value 	0Bh
			break;
		case 0x0C: //FOV CHANGE MODE Slow / Fast 	0Ch
			break;
		case 0x0D: //VIDEO FORMAT Analog / Digital 	0Dh
			break;
		case 0x0E: //DEFOG Off / On 	0Eh
			break;
		case 0x0F: //FOV Set FOV factor 	0Fh
			break;
		case 0x11: //EXPLOSURE MODE Manual / Auto 	11h
			break;
		case 0x21: //FOV Zoom in 	21h
			break;
		case 0x22: //FOV Zoom out 	22h
			break;
		case 0x23: //FOV Zoom (position) 	23h
			break;
		case 0x24: //FOV Stop Zoom 	24h
			break;
		case 0x25: //FOCUS Focus out (to near) 	25h
			break;
		case 0x26: //FOCUS Focus in (to far) 	26h
			break;
		case 0x27: //FOCUS Focus Stop 	27h
			break;
		case 0x28: //FOCUS Manual or Auto focus	28h
			break;
		case 0x29: //POLARITY Black hot 	29h
			break;
		case 0x2A: //POLARITY White hot 	2Ah
			break;
		case 0x2B: //0xBRIGHTNESS Set brightness value 	2Bh
			break;
		case 0x2C: //CONTRAST Set contrast value 	2Ch
			break;
		case 0x41: //GENERAL CCD-FLIR FOV COORDINATION No coordination 	41h
			break;
		case 0x42: //GENERAL CCD-FLIR FOV COORDINATION FLIR is master	42h
			break;
		case 0x43: //GENERAL CCD-FLIR FOV COORDINATION CMOS is master	43h
			break;
		case 0x44: //FLIR/CMOS/LRF Boresight Get table of misalignments	44h
			break;
		case 0x51: //LRF Enable Disable 	51h
			break;
		case 0x52: //LRF Enable Enable (=Armed)	52h
			break;
		case 0x53: //LRF Enable Fire	53h
			break;
		case 0x54: //Range Set lower limit 	54h
			break;
		case 0x55: //Range Set upper limit 	55h
			break;

		}
	}

    if (data.empty()) {
        std::cout << "Received empty data" << std::endl;
        SendCommand(rxBuffer, 8);
        return;
    }
    SendCommand(rxBuffer, 8);

    std::cout << "Client received data: ";
    for (uint8_t byte : data) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;
    //UartDevice::StartReceive(&rxMsg, rxMsg.SIZE_BYTES);
    // Add further parsing or handling logic here
}





#include "Client.hpp"
#include <cstdio>
#include "task.h"
#include "DayCam.hpp"  // Include the actual DayCam class
#include "LRX20A.hpp"
#include "RPLens.hpp"
#include "IRay.hpp"
#include "cmsis_os2.h"


//typedef union {
//    uint16_t value;   // packed 32-bit register/value
//    struct {
//        unsigned COOLING_ON        : 1;  // bit 0
//        unsigned HEATER_SDI_CONV   : 1;  // bit 1
//        unsigned HEATER_ON_IRCAM   : 1;  // bit 2
//        unsigned HEATER_MISC       : 1;  // bit 3
//        unsigned DAYCAM_TEC_ON     : 1;  // bit 4
//        unsigned DAYCAM_COOL_HEAT  : 1;  // bit 5 (0=cool,1=heat) if that's your meaning
//        unsigned IR_TEC_ON         : 1;  // bit 6
//        unsigned IR_COOL_HEAT      : 1;  // bit 7
//        unsigned RP_TEC_ON         : 1;  // bit 8
//        unsigned RP_COOL_HEAT      : 1;  // bit 9
//        unsigned _reserved         : 6; // bits 10..15
//    } bits;
//} ctrlReg_t;

extern uint32_t setCtrlReg (ctrlReg_t reg);
extern uint32_t clrCtrlReg (ctrlReg_t reg);


Client::Client(UART_HandleTypeDef* huart)
    : UartEndpoint(huart) {}


void Client::Init() {


	const osThreadAttr_t clientTaskAttr = {
	    .name = "ClientTask",
	    .stack_size = 1024 * 4,  // in bytes
	    .priority = (osPriority_t) osPriorityNormal
	};

	osThreadNew([](void* arg) {
	    static_cast<Client*>(arg)->processIncomingThread();
	}, this, &clientTaskAttr);





    if (!StartReceive(&byte_, 1)) {
        printf("Client StartReceive failed\r\n");


    }
    else {
    	printf("Client StartReceive success\r\n");
        rxState_ = RxState::Ready;
    }
}



void Client::onReceiveByte(uint8_t byte) {
	const TickType_t now = xTaskGetTickCount();

	switch (rxState_)
	{

	case RxState::Ready:
    // Start of new message
        if (byte == 0xAA) { // Assume HEADER
            buffer_[0] = byte;
            bufferIndex_ = 1;
            firstByteTick_ = now;
            rxState_ = RxState::Receiving;
        }
     break;

	case RxState::Receiving:

		// Timeout check
		if ((now - firstByteTick_) > pdMS_TO_TICKS(500)) {
			// Start of new message
			if (byte == 0xAA) { // Assume HEADER
				rxMsg.m_Header = byte;
				buffer_[0] = byte;
				bufferIndex_ = 1;
				firstByteTick_ = now;
				rxState_ = RxState::Receiving;
			}
			else
				resetReception();  // Clear buffer, reset flags
			break;
		}

		// Accumulate byte
		if (bufferIndex_ < sizeof(buffer_)) {
			buffer_[bufferIndex_++] = byte;
		} else {
			resetReception();
			break;
		}

		// Wait until we have at least length
		if (bufferIndex_ == 6) {
			expectedLength_ = 6 + buffer_[5] + 2; // 6 header+meta + payload + CRC+FOOTER
		}

		if (bufferIndex_ >= 6 && bufferIndex_ == expectedLength_) {
			if (buffer_[expectedLength_-1] == 0x55) { // FOOTER
//				if (verifyCRC(buffer_, expectedLength_)) {
//					parseAndProcess(buffer_, expectedLength_);
//				}
				if (verifyCRC(buffer_, expectedLength_)) {

	                rxMsg.m_Header  = buffer_[0];
	                rxMsg.m_srcID   = buffer_[1];
	                rxMsg.m_destID  = buffer_[2];
	                rxMsg.m_opCode  = buffer_[3];
	                rxMsg.m_addr    = buffer_[4];
	                rxMsg.m_length  = buffer_[5];

	                if (rxMsg.m_length <= 64) {  // sanity check
	                    rxMsg.m_payload.assign(buffer_ + 6, buffer_ + 6 + rxMsg.m_length);
	                    rxMsg.m_dataCRC = buffer_[6 + rxMsg.m_length];
	                    rxMsg.m_Footer  = buffer_[6 + rxMsg.m_length + 1];
	                    rxMsgQuee_.push_back(rxMsg);
	                }
				}
			}
			resetReception();
		}
		break;


	case RxState::Stopped:

	// Ignore all incoming data
	break;

	case RxState::Idle:
		break;
	default:
		break;
	}

}

void Client::resetReception() {
    bufferIndex_ = 0;
    receiving_ = false;
    rxState_ = RxState::Ready;
    firstByteTick_ = 0;
}

void Client::processIncoming() {
    // Example logic (depending on your architecture)
    while (!rxQueue_.empty()) {
        auto msg = rxQueue_.front();
        rxQueue_.pop_front();
        parseAndProcess(reinterpret_cast<uint8_t*>(&msg), sizeof(comm::Message));
    }
}

void Client::parseAndProcess(uint8_t* msg, size_t len) {
    uint8_t srcID = msg[1];
    uint8_t destID = msg[2];
    uint8_t opCode = msg[3];
//    uint8_t addr   = msg[4];
    uint8_t length = msg[5];
    uint8_t* payload = &msg[6];

    // You can now switch based on opCode, etc.
    switch (opCode) {
        case 0x01:
        	//dayCam_->SendCommand(dayCam_->zoom_teleVar, sizeof(dayCam_->zoom_teleVar));
        	if (payload != nullptr)
        		dayCam_->handleZoomIn(payload, length);

        	    printf("Zoom sent in\r\n");
            break;
        case 0x02:
        	if (payload != nullptr)
        		dayCam_->handleZoomOut(payload, length);
        		printf("Zoom sent out\r\n");
            break;
        case 0x03:
        	if (payload != nullptr){
        	    uint16_t position = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
        	    dayCam_->handleZoom2Position(position);
        	}
            break;
        case 0x04:
        	dayCam_->handleZoomStop();
			break;
        case 0x05:
        	//dayCam_->SendCommand(dayCam_->zoom_teleVar, sizeof(dayCam_->zoom_teleVar));
        	if (payload != nullptr)
        		dayCam_->handleFocusNear(payload, length);
            break;
        case 0x06:
        	if (payload != nullptr)
        		dayCam_->handleFocusFar(payload, length);
            break;
        case 0x07:
        	if (payload != nullptr){
        	    uint16_t position = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
        	    dayCam_->handleFocus2Position(position);
        	}
            break;
        case 0x08:
        	dayCam_->handleFocusStop();
			break;

        case 0x21:
        	rpLens_->handleZoomIn();
    		printf("RP Lens Zoom in\r\n");
			break;

        case 0x22:
        	rpLens_->handleZoomOut();
    		printf("RP Lens Zoom out\r\n");
			break;

        case 0x24:
        	rpLens_->handleZoomStop();
    		printf("RP Lens Zoom stop\r\n");
			break;


        case 0x51: //LRF Disable
        	//lrx20A_->RangesDataCommand();
			break;
        case 0x52: //LRF Enable
        	//lrx20A_->RangesDataCommand();
			break;
        case 0x53: //LRF Enable Fire
        	lrx20A_->RangesDataCommand();
			break;
        case 0x54: //LRF Set lower limit
        	lrx20A_->SetMinimumRangeCommand();
			break;
        case 0x55: //LRF Set upper limit
        	lrx20A_->SetMaximumRangeCommand();
			break;

        case 0x70: //Set reg


 //       	ctrlReg_t tmpReg;

        	tmpReg.value = tmpReg.value | ((static_cast<uint16_t>(payload[0]) << 8) | payload[1]);
        	printf("CTRL register set 1\r\n");


        	if (tmpReg.bits.COOLING_ON) 		HAL_GPIO_WritePin(	COOLING_ON_GPIO_Port, 		COOLING_ON_Pin, 		GPIO_PIN_SET);
        	if (tmpReg.bits.DAYCAM_COOL_HEAT) 	HAL_GPIO_WritePin(	DAYCAM_COOL_HEAT_GPIO_Port ,DAYCAM_COOL_HEAT_Pin, 	GPIO_PIN_SET);
        	if (tmpReg.bits.HEATER_ON_IRCAM) 	HAL_GPIO_WritePin(	HEATER_ON_IRCAM_GPIO_Port, 	HEATER_ON_IRCAM_Pin, 	GPIO_PIN_SET);
        	if (tmpReg.bits.HEATER_SDI_CONV) 	HAL_GPIO_WritePin(	HEATER_SDI_CONV_GPIO_Port, 	HEATER_SDI_CONV_Pin, 	GPIO_PIN_SET);
        	if (tmpReg.bits.HEATER_MISC) 		HAL_GPIO_WritePin(	HEATER_MISC_GPIO_Port, 		HEATER_MISC_Pin, 		GPIO_PIN_SET);
        	if (tmpReg.bits.DAYCAM_TEC_ON) 		HAL_GPIO_WritePin(	DAYCAM_TEC_ON_GPIO_Port, 	DAYCAM_TEC_ON_Pin, 		GPIO_PIN_SET);
        	if (tmpReg.bits.IR_TEC_ON) 			HAL_GPIO_WritePin(	IR_TEC_ON_GPIO_Port, 		IR_TEC_ON_Pin, 			GPIO_PIN_SET);
        	if (tmpReg.bits.IR_COOL_HEAT) 		HAL_GPIO_WritePin(	IR_COOL_HEAT_GPIO_Port, 	IR_COOL_HEAT_Pin, 		GPIO_PIN_SET);
        	if (tmpReg.bits.RP_TEC_ON) 			HAL_GPIO_WritePin(	RP_TEC_ON_GPIO_Port, 		RP_TEC_ON_Pin, 			GPIO_PIN_SET);
        	if (tmpReg.bits.RP_COOL_HEAT) 		HAL_GPIO_WritePin(	RP_COOL_HEAT_GPIO_Port, 	RP_COOL_HEAT_Pin, 		GPIO_PIN_SET);

        	printf("CTRL register set 2\r\n");
			break;
        case 0x71: //Set reg

        	tmpReg.value = tmpReg.value & ((static_cast<uint16_t>(payload[0]) << 8) | payload[1]);

        	if (!tmpReg.bits.COOLING_ON) 		HAL_GPIO_WritePin(	COOLING_ON_GPIO_Port, 		COOLING_ON_Pin, 		GPIO_PIN_RESET);
        	if (!tmpReg.bits.DAYCAM_COOL_HEAT) 	HAL_GPIO_WritePin(	DAYCAM_COOL_HEAT_GPIO_Port ,DAYCAM_COOL_HEAT_Pin, 	GPIO_PIN_RESET);
        	if (!tmpReg.bits.HEATER_ON_IRCAM) 	HAL_GPIO_WritePin(	HEATER_ON_IRCAM_GPIO_Port, 	HEATER_ON_IRCAM_Pin, 	GPIO_PIN_RESET);
        	if (!tmpReg.bits.HEATER_SDI_CONV) 	HAL_GPIO_WritePin(	HEATER_SDI_CONV_GPIO_Port, 	HEATER_SDI_CONV_Pin, 	GPIO_PIN_RESET);
        	if (!tmpReg.bits.HEATER_MISC) 		HAL_GPIO_WritePin(	HEATER_MISC_GPIO_Port, 		HEATER_MISC_Pin, 		GPIO_PIN_RESET);
        	if (!tmpReg.bits.DAYCAM_TEC_ON) 	HAL_GPIO_WritePin(	DAYCAM_TEC_ON_GPIO_Port, 	DAYCAM_TEC_ON_Pin, 		GPIO_PIN_RESET);
        	if (!tmpReg.bits.IR_TEC_ON) 		HAL_GPIO_WritePin(	IR_TEC_ON_GPIO_Port, 		IR_TEC_ON_Pin, 			GPIO_PIN_RESET);
        	if (!tmpReg.bits.IR_COOL_HEAT) 		HAL_GPIO_WritePin(	IR_COOL_HEAT_GPIO_Port, 	IR_COOL_HEAT_Pin, 		GPIO_PIN_RESET);
        	if (!tmpReg.bits.RP_TEC_ON) 		HAL_GPIO_WritePin(	RP_TEC_ON_GPIO_Port, 		RP_TEC_ON_Pin, 			GPIO_PIN_RESET);
        	if (!tmpReg.bits.RP_COOL_HEAT) 		HAL_GPIO_WritePin(	RP_COOL_HEAT_GPIO_Port, 	RP_COOL_HEAT_Pin, 		GPIO_PIN_RESET);



        	printf("CTRL register clear\r\n");
			break;

        case 0x72: //Set reg
			iRay_->Reticle4();
        	printf("iRay Reticle4\r\n");
			break;

        default:
            break;
    }
}



void Client::processIncomingThread() {
	RxMessage msg;

	while (true) {
//		if (osMessageQueueGet(rxMsgQueue_, &msg, nullptr, osWaitForever) == osOK) {
//			parseAndProcess(msg.data, msg.length);
//			vPortFree(msg.data);  // Free after processing
//		}
		if (!rxMsgQuee_.empty())
		{

	        comm::Message msg = rxMsgQuee_.front();
	        rxMsgQuee_.pop_front();



	        uint8_t buffer[sizeof(msg)];  // Or adjust size appropriately

	        msg.rawBuffer(buffer);

	        size_t len = sizeof(buffer);  // Fill the buffer and get actual size

	        parseAndProcess(buffer, len);  // Now pass it to your parser

//		HAL_GPIO_TogglePin(USER_LED3_GPIO_Port, USER_LED3_Pin);
		//vTaskDelay(pdMS_TO_TICKS(5000));
		//osDelay(5000);
		}
	}
}







bool Client::verifyCRC(uint8_t* msg, size_t len) {
    if (len < 3) return false;  // Must have at least header + CRC + footer

    uint8_t crc = 0x00;  // or 0xFF depending on your protocol
    for (size_t i = 1; i < len - 2; ++i) { // exclude CRC and footer
        crc ^= msg[i];

    }

    uint8_t receivedCRC = msg[len - 2];
    return crc == receivedCRC;
}

void Client::setDayCam(DayCam* cam) {
    dayCam_ = cam;
}

void Client::setLRF(LRX20A* lrf) {
    lrx20A_ = lrf;
}

void Client::setIRay(IRay* cam) {
    iRay_ = cam;
}

void Client::setRPLens(RPLens* lens) {
    rpLens_ = lens;
}

uint32_t setCtrlReg (ctrlReg_t reg)
{

	if (reg.bits.COOLING_ON) HAL_GPIO_WritePin(COOLING_ON_GPIO_Port, COOLING_ON_Pin, GPIO_PIN_SET);
//	if (reg.bits.HEATER_SDI_CONV) HAL_GPIO_WritePin(HEATER_SDI_CONV_GPIO_Port, HEATER_SDI_CONV_Pin, GPIO_PIN_SET);
//	if (reg.bits.HEATER_MISC) HAL_GPIO_WritePin(HEATER_MISC_GPIO_Port, HEATER_MISC_Pin, GPIO_PIN_SET);
//	if (reg.bits.DAYCAM_TEC_ON) HAL_GPIO_WritePin(DAYCAM_TEC_ON_GPIO_Port, DAYCAM_TEC_ON_Pin, GPIO_PIN_SET);
//	if (reg.bits.IR_TEC_ON) HAL_GPIO_WritePin(IR_TEC_ON_GPIO_Port, IR_TEC_ON_Pin, GPIO_PIN_SET);
//	if (reg.bits.IR_COOL_HEAT) HAL_GPIO_WritePin(IR_COOL_HEAT_GPIO_Port, IR_COOL_HEAT_Pin, GPIO_PIN_SET);
//	if (reg.bits.RP_TEC_ON) HAL_GPIO_WritePin(RP_TEC_ON_GPIO_Port, RP_TEC_ON_Pin, GPIO_PIN_SET);
//	if (reg.bits.RP_COOL_HEAT) HAL_GPIO_WritePin(RP_COOL_HEAT_GPIO_Port, RP_COOL_HEAT_Pin, GPIO_PIN_SET);

}

uint32_t clrCtrlReg (ctrlReg_t reg)
{

	if (!reg.bits.COOLING_ON) HAL_GPIO_WritePin(COOLING_ON_GPIO_Port, COOLING_ON_Pin, GPIO_PIN_RESET);
//	if (!reg.bits.HEATER_SDI_CONV) HAL_GPIO_WritePin(HEATER_SDI_CONV_GPIO_Port, HEATER_SDI_CONV_Pin, GPIO_PIN_RESET);
//	if (!reg.bits.HEATER_MISC) HAL_GPIO_WritePin(HEATER_MISC_GPIO_Port, HEATER_MISC_Pin, GPIO_PIN_RESET);
//	if (!reg.bits.DAYCAM_TEC_ON) HAL_GPIO_WritePin(DAYCAM_TEC_ON_GPIO_Port, DAYCAM_TEC_ON_Pin, GPIO_PIN_RESET);
//	if (!reg.bits.IR_TEC_ON) HAL_GPIO_WritePin(IR_TEC_ON_GPIO_Port, IR_TEC_ON_Pin, GPIO_PIN_RESET);
//	if (!reg.bits.IR_COOL_HEAT) HAL_GPIO_WritePin(IR_COOL_HEAT_GPIO_Port, IR_COOL_HEAT_Pin, GPIO_PIN_RESET);
//	if (!reg.bits.RP_TEC_ON) HAL_GPIO_WritePin(RP_TEC_ON_GPIO_Port, RP_TEC_ON_Pin, GPIO_PIN_RESET);
//	if (!reg.bits.RP_COOL_HEAT) HAL_GPIO_WritePin(RP_COOL_HEAT_GPIO_Port, RP_COOL_HEAT_Pin, GPIO_PIN_RESET);

}




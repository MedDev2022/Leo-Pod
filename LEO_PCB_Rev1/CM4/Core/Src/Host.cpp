#include "Host.hpp"
#include "DayCam.hpp"
#include "LRX20A.hpp"
#include "RPLens.hpp"
#include "IRay.hpp"
#include  "CLI.hpp"
#include <cstdio>
#include "task.h"
#include "comm.hpp"


extern UART_HandleTypeDef* g_DebugUart;



Host::Host(UART_HandleTypeDef* huart)
    : UartEndpoint(huart, "HostTask") {

    // Create the software timer (one-shot, byte receive timeout)
    timeoutTimer_ = xTimerCreate(
        "HostTimeout",              // Timer name
        pdMS_TO_TICKS(TIMEOUT_MS),  // Timer period (byte receive timeout)
        pdFALSE,                    // One-shot timer (not auto-reload)
        this,                       // Timer ID (pointer to this object)
        timeoutTimerCallback        // Callback function
    );

    if (timeoutTimer_ == nullptr) {
        printf("Host: Failed to create timeout timer\r\n");
    }
}



void Host::Init() {
    if (!StartReceive()) {
        printf("Host Start Receive failed\r\n");
    } else {
        printf("Host Start Receive success\r\n");
        rxState_ = RxState::Ready;
    }
}

// Static callback function for the timer
void Host::timeoutTimerCallback(TimerHandle_t xTimer) {
    // Get the Host object pointer from timer ID
    Host* host = static_cast<Host*>(pvTimerGetTimerID(xTimer));
    if (host != nullptr) {
        host->handleTimeout();
    }
}

// Start the timeout timer
void Host::startTimeoutTimer() {
    if (timeoutTimer_ != nullptr) {
        // Reset and start the timer from ISR context
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerResetFromISR(timeoutTimer_, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// Stop the timeout timer
void Host::stopTimeoutTimer() {
    if (timeoutTimer_ != nullptr) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerStopFromISR(timeoutTimer_, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// Handle timeout event
void Host::handleTimeout() {
	stopTimeoutTimer();
    printf("Host: byte receive timeout triggered! bufferIndex_=%u, transparent=%d\r\n",
           bufferIndex_, (destHuart_ != nullptr));

    // Only handle timeout if we're actually receiving
    if (rxState_ != RxState::Receiving || bufferIndex_ == 0) {
        printf("Host: Exited from timer handler RxState=%u BafferIndex=%u\r\n", (rxState_ == RxState::Receiving),bufferIndex_);
        return;
    }

    // Print what we have
    printf("  Data: ");
    for (uint8_t i = 0; i < bufferIndex_ && i < 16; i++) {
        printf("%02X ", buffer_[i]);
    }
    printf("\r\n");

    // Handle based on mode
    if (destHuart_ != nullptr) {
        // Transparent mode - flush to destination
        printf("Host: [Transparent] Timeout - flushing to destination\r\n");
        HAL_UART_Transmit(destHuart_, buffer_, bufferIndex_, 100);
    } else {
        // Normal mode - discard
        printf("Host: [Normal] Timeout - discarding incomplete data\r\n");
    }

    resetReception();
}

    void Host::processRxData(uint8_t byte) {
        const TickType_t now = xTaskGetTickCount();

//        printf("Host: RX byte=0x%02X state=%d bufIdx=%u transparent=%d\r\n",
//        		byte, (int)rxState_, bufferIndex_, (destHuart_ != nullptr));

        // Handle transparent mode - SNIFF for commands
        if (destHuart_ != nullptr) {
            //Always check for header even in transparent mode
            if (byte == 0xAA && rxState_ == RxState::Ready) {
                // Potential command header detected
                buffer_[0] = byte;
                bufferIndex_ = 1;
                firstByteTick_ = now;
                rxState_ = RxState::Receiving;

                printf("Host: [Transparent] Header detected, timer started\r\n");

                // Start byte receive timeout timer
                startTimeoutTimer();
                return;  // Don't forward this byte yet
            }

            // If collecting a potential command
            if (rxState_ == RxState::Receiving) {
                // Reset timer on each byte received
                startTimeoutTimer();


                // Accumulate byte
                if (bufferIndex_ < sizeof(buffer_)) {
                    buffer_[bufferIndex_++] = byte;
                    printf("Host: [Transparent] Collecting byte[%u]=0x%02X\r\n",
                           bufferIndex_-1, byte);
                } else {
                    // Buffer overflow - flush everything and reset
                    stopTimeoutTimer();
                    printf("Host: [Transparent] Buffer overflow, flushing\r\n");
                    HAL_UART_Transmit(destHuart_, buffer_, bufferIndex_, 100);
                    resetReception();
                    HAL_UART_Transmit(destHuart_, &byte, 1, 100);
                    return;
                }

                // Parse length when we have it
                if (bufferIndex_ == 6) {
                    expectedLength_ = 6 + buffer_[5] + 2;

                    if (expectedLength_ > sizeof(buffer_)) {
                        // Invalid length - flush everything
                        stopTimeoutTimer();
                        printf("Host: [Transparent] Invalid length, flushing\r\n");
                        HAL_UART_Transmit(destHuart_, buffer_, bufferIndex_, 100);
                        resetReception();
                        return;
                    }
                    printf("Host: [Transparent] Expecting %u total bytes\r\n", expectedLength_);
                }

                // Check if message complete
                if (bufferIndex_ >= 6 && bufferIndex_ == expectedLength_) {
                	stopTimeoutTimer();  // Stop timer immediately


                    // Verify footer
                    if (buffer_[expectedLength_-1] != 0x55) {
                        printf("Host: [Transparent] Bad footer, flushing\r\n");
                        HAL_UART_Transmit(destHuart_, buffer_, bufferIndex_, 100);
                        resetReception();
                        return;
                    }

                    // Verify CRC
                    if (!verifyCRC(buffer_, expectedLength_)) {
                        printf("Host: [Transparent] CRC failed, flushing\r\n");
                        HAL_UART_Transmit(destHuart_, buffer_, bufferIndex_, 100);
                        resetReception();
                        return;
                    }

                    // VALID COMMAND - Check if it's a control command
                    comm::Message rxMsg;
                    rxMsg.m_Header  = buffer_[0];
                    rxMsg.m_srcID   = buffer_[1];
                    rxMsg.m_destID  = buffer_[2];
                    rxMsg.m_opCode  = buffer_[3];
                    rxMsg.m_addr    = buffer_[4];
                    rxMsg.m_length  = buffer_[5];

                    if (rxMsg.m_length <= 64) {
                        rxMsg.m_payload.assign(buffer_ + 6, buffer_ + 6 + rxMsg.m_length);
                        rxMsg.m_dataCRC = buffer_[6 + rxMsg.m_length];
                        rxMsg.m_Footer  = buffer_[6 + rxMsg.m_length + 1];

                        // Check if it's a transparent mode control command
                        if (rxMsg.m_opCode == 0x80) {
                            // Switch device command
                            printf("Host: [Transparent] Switch device command received\r\n");
                            parseAndProcess(rxMsg);



                            // Print complete message
                            printf("  Full msg: ");
                            for (uint8_t i = 0; i < expectedLength_; i++) {
                                printf("%02X ", buffer_[i]);
                            }
                            printf("\r\n");

                            resetReception();
                            return;
                        }
                        else if (rxMsg.m_opCode == 0x81) {
                            // Exit transparent mode command
                            printf("Host: [Transparent] Exit transparent mode command received\r\n");
                            parseAndProcess(rxMsg);
                            resetReception();
                            return;
                        }
                        else {
                            // Not a control command - forward entire message to destination
                            printf("Host: [Transparent] Not a control cmd (0x%02X), forwarding\r\n",
                                   rxMsg.m_opCode);
                            HAL_UART_Transmit(destHuart_, buffer_, bufferIndex_, 100);
                            resetReception();
                            return;
                        }
                    }
                }


                // Still collecting - don't forward yet
                return;
            }

            // Normal transparent mode - forward immediately
            HAL_UART_Transmit(destHuart_, &byte, 1, 100);
            return;
        }

        else
        {
        // Normal mode (not transparent)
        switch (rxState_) {
            case RxState::Ready:
                if (byte == 0xAA) {  // Header detected
                    buffer_[0] = byte;
                    bufferIndex_ = 1;
                    firstByteTick_ = now;
                    rxState_ = RxState::Receiving;

                    // Start byte receive timeout timer
                    startTimeoutTimer();


                    printf("Host: Header detected, timer started\r\n");
                } else {
                    // Ignore bytes when waiting for header
                    printf("Host: Waiting for header, ignoring 0x%02X\r\n", byte);
                }
                break;

            case RxState::Receiving: {
                // Reset timer on each byte received
                startTimeoutTimer();

                // Accumulate byte
                if (bufferIndex_ < sizeof(buffer_)) {
                    buffer_[bufferIndex_++] = byte;
                        printf("Host: Stored byte[%u]=0x%02X\r\n",
                              bufferIndex_-1, byte);
                } else {
                    printf("Host: Buffer overflow! bufferIndex_=%u\r\n", bufferIndex_);
                    stopTimeoutTimer();
                    resetReception();
                    break;
                }

                // Parse length when we have it
                if (bufferIndex_ == 6) {
                    expectedLength_ = 6 + buffer_[5] + 2;  // header(1) + fields(5) + payload + CRC(1) + footer(1)

                    if (expectedLength_ > sizeof(buffer_)) {
                        printf("Host: Invalid length=%u (payload=%u)\r\n",
                               expectedLength_, buffer_[5]);
                        stopTimeoutTimer();
                        resetReception();
                        break;
                    }
                    printf("Host: Expecting %u total bytes (payload=%u)\r\n",
                           expectedLength_, buffer_[5]);
                }

                // Check if message complete
                if (bufferIndex_ >= 6 && bufferIndex_ == expectedLength_) {
                    printf("Host: Message complete! Verifying...\r\n");

                    // Stop timer - message is complete
                    stopTimeoutTimer();

                    // Print complete message
                    printf("  Full msg: ");
                    for (uint8_t i = 0; i < expectedLength_; i++) {
                        printf("%02X ", buffer_[i]);
                    }
                    printf("\r\n");

                    // Verify footer
                    if (buffer_[expectedLength_-1] != 0x55) {
                        printf("Host: Bad footer=0x%02X (expected 0x55)\r\n",
                               buffer_[expectedLength_-1]);
                        resetReception();
                        break;
                    }

                    // Verify CRC
                    if (!verifyCRC(buffer_, expectedLength_)) {
                        printf("Host: CRC failed\r\n");
                        resetReception();
                        break;
                    }

                    // VALID MESSAGE - Parse it
                    comm::Message rxMsg;
                    rxMsg.m_Header  = buffer_[0];
                    rxMsg.m_srcID   = buffer_[1];
                    rxMsg.m_destID  = buffer_[2];
                    rxMsg.m_opCode  = buffer_[3];
                    rxMsg.m_addr    = buffer_[4];
                    rxMsg.m_length  = buffer_[5];

                    if (rxMsg.m_length <= 64) {
                        rxMsg.m_payload.assign(buffer_ + 6, buffer_ + 6 + rxMsg.m_length);
                        rxMsg.m_dataCRC = buffer_[6 + rxMsg.m_length];
                        rxMsg.m_Footer  = buffer_[6 + rxMsg.m_length + 1];

                        printf("Host: Valid message! OpCode=0x%02X, Length=%u\r\n",
                               rxMsg.m_opCode, rxMsg.m_length);

                        parseAndProcess(rxMsg);
                    }

                    resetReception();
                }
                break;
            }

            default:
                printf("Host: Unknown state %d\r\n", (int)rxState_);
                stopTimeoutTimer();
                resetReception();
                break;
        }
        }
    }


void Host::parseAndProcess(const comm::Message& msg) {
    const uint8_t* payload = msg.m_payload.data();
    const uint8_t length = msg.m_length;

    switch (msg.m_opCode) {
        case 0x01:  // Zoom In
            if (dayCam_ && payload != nullptr) {
                dayCam_->handleZoomIn(const_cast<uint8_t*>(payload), length);
                printf("DayCam Zoom In\r\n");
            }
            break;

        case 0x02:  // Zoom Out
            if (dayCam_ && payload != nullptr) {
                dayCam_->handleZoomOut(const_cast<uint8_t*>(payload), length);
                printf("DayCam Zoom Out\r\n");
            }
            break;

        // ... Add all other cases from your original implementation

        case 0x70:  // Set control register
            tmpReg.value |= ((static_cast<uint16_t>(payload[0]) << 8) | payload[1]);

            if (tmpReg.bits.COOLING_ON)
                HAL_GPIO_WritePin(COOLING_ON_GPIO_Port, COOLING_ON_Pin, GPIO_PIN_SET);
            if (tmpReg.bits.DAYCAM_TEC_ON)
                HAL_GPIO_WritePin(DAYCAM_TEC_ON_GPIO_Port, DAYCAM_TEC_ON_Pin, GPIO_PIN_SET);
            // ... etc
            printf("Control register set\r\n");
            break;

        case 0x80:  // Enter transparent mode forward to DayCam
            if (length == 1) {
                switch (payload[0]) {
                    case 0x01:

                    	if (dayCam_ != nullptr)
                    	{
                    		this->destHuart_ = dayCam_->huart_;
                    		dayCam_->setTransparentMode(true, this->huart_);
                            iRay_->setTransparentMode(false, nullptr);
                            lrx20A_->setTransparentMode(false, nullptr);
                            rpLens_->setTransparentMode(false, nullptr);
                    		 printf("Host switched to DayCam\r\n");
                    	}
                    	else
                    	{
                    		this->destHuart_ =nullptr;
                    		printf("Host switch to DayCam failed\r\n");
                    	}
                        break;

                     case 0x02:

                      	if (iRay_ != nullptr)
                         	{
                         		this->destHuart_ = iRay_->huart_;
                         		dayCam_->setTransparentMode(false, nullptr);
                         		iRay_->setTransparentMode(true, this->huart_);
                                lrx20A_->setTransparentMode(false, nullptr);
                                rpLens_->setTransparentMode(false, nullptr);
                         		printf("Host switched to iRay\r\n");
                         	}
                         	else
                         	{
                         		this->destHuart_ =nullptr;
                         		printf("Host switch to iRay failed\r\n");
                         	}
                         break;

                     case 0x03:
                         this->destHuart_ = (lrx20A_ != nullptr) ? lrx20A_->huart_ : nullptr;
                         dayCam_->setTransparentMode(false, nullptr);
                         iRay_->setTransparentMode(false, nullptr);
                         lrx20A_->setTransparentMode(true, this->huart_);
                         rpLens_->setTransparentMode(false, nullptr);
                         printf("Host switched to LRF transparent mode\r\n");
                         break;

                     case 0x04:
                         this->destHuart_ = (rpLens_ != nullptr) ? rpLens_->huart_ : nullptr;
                         dayCam_->setTransparentMode(false, nullptr);
                         iRay_->setTransparentMode(false, nullptr);
                         lrx20A_->setTransparentMode(false, nullptr);
                         rpLens_->setTransparentMode(true, this->huart_);
                         printf("Host switched to RRlens transparent mode\r\n");
                         break;

                     case 0x00:
                         this->destHuart_ = nullptr;
                         dayCam_->setTransparentMode(false, nullptr);
                         iRay_->setTransparentMode(false, nullptr);
                         lrx20A_->setTransparentMode(false, nullptr);
                         rpLens_->setTransparentMode(false, nullptr);
                         printf("Host exit transparent mode\r\n");
                         break;
                     // ... other cases
                 }

             }
             break;

        case 0x81:  // Exit transparent mode
            if (length == 0) {
                this->destHuart_ = nullptr;
                dayCam_->setTransparentMode(false, nullptr);
                iRay_->setTransparentMode(false, nullptr);
                lrx20A_->setTransparentMode(false, nullptr);
                rpLens_->setTransparentMode(false, nullptr);
                printf("Host exit transparent mode\r\n");
                break;
            }
            break;

        case 0x82:  // Debug to Host/CLI
            if (length == 1 && payload != nullptr) {
            	switch (payload[0])
            	{
            	case 0:
            		printf("Debug forwarded to CLI\r\n");
            		g_DebugUart = cli_->huart_;
            		break;
            	case 1:
            		printf("Debug forwarded to HOST\r\n");
            		g_DebugUart = this->huart_;
            		break;
            	}
//                this->destHuart_ = nullptr;
//                dayCam_->setTransparentMode(false, nullptr);
//                iRay_->setTransparentMode(false, nullptr);
//                lrx20A_->setTransparentMode(false, nullptr);
//                rpLens_->setTransparentMode(false, nullptr);
//                printf("Host exit transparent mode\r\n");
//                break;
//
//                if (dayCam_ && payload != nullptr) {
//                    dayCam_->handleZoomOut(const_cast<uint8_t*>(payload), length);
//                    printf("DayCam Zoom Out\r\n");
               }

            break;

        case 0x90: //Get temperatures

        	buffer_[0]  = 0xAA;
        	buffer_[1]  = 0x70;
        	buffer_[2]  = 0x10;
        	buffer_[3]  = 0x90;
        	buffer_[4]    = 0x00;
        	buffer_[5]  = 8;
        	buffer_[6] = Temp[0];
        	buffer_[7] = Temp[0]>>8;
        	buffer_[8] = Temp[1];
        	buffer_[9] = Temp[1]>>8;
        	buffer_[10] = Temp[2];
        	buffer_[11] = Temp[2]>>8;
        	buffer_[12] = Temp[3];
        	buffer_[13] = Temp[3]>>8;
        	buffer_[14] = txMsg.calcCRC(&txMsg.m_Header + sizeof(txMsg.m_Header), 6 + txMsg.m_length);
        	buffer_[15] = 0x55;



            this->SendCommand(this->buffer_,this->buffer_[5]+8);



            break;


        case 0x91: //Get IMU

        	buffer_[0]  = 0xAA;
        	buffer_[1]   = 0x70;
        	buffer_[2]  = 0x10;
        	buffer_[3]  = 0x91;
        	buffer_[4]    = 0x00;
        	buffer_[5]  = 6;
        	buffer_[6]  = Imu[0];
        	buffer_[7]  = Imu[0]>>8;
        	buffer_[8]  = Imu[1];
        	buffer_[9]  = Imu[1]>>8;
        	buffer_[10] = Imu[2];
        	buffer_[11] = Imu[2]>>8;
        	buffer_[12] = txMsg.calcCRC(&txMsg.m_Header + sizeof(txMsg.m_Header), 6 + txMsg.m_length);
        	buffer_[13] = 0x55;
            this->SendCommand(this->buffer_,this->buffer_[5]+8);


            break;
    }
}

void Host::resetReception() {

	stopTimeoutTimer();

    // DRAIN THE QUEUE (This is the key!)
    uint8_t dummy;
//    while (osMessageQueueGet(rxQueue_, &dummy, nullptr, 0) == osOK) {
//        // Discard any leftover bytes
//    }

    // Reset index and state
    bufferIndex_ = 0;
    rxState_ = RxState::Ready;
    firstByteTick_ = 0;

    // Clear expected length (important for next message)
    expectedLength_ = 0;

    // Clear buffer to prevent old data contamination
    memset(buffer_, 0, sizeof(buffer_));
}

bool Host::verifyCRC(uint8_t* msg, size_t len) {
    if (len < 3) return false;

    uint8_t crc = 0x00;
    for (size_t i = 1; i < len - 2; ++i) {
        crc ^= msg[i];
    }


    return crc == msg[len - 2];
}

void Host::setDayCam(DayCam* cam) { dayCam_ = cam; }
void Host::setLRF(LRX20A* lrf) { lrx20A_ = lrf; }
void Host::setIRay(IRay* cam) { iRay_ = cam; }
void Host::setRPLens(RPLens* lens) { rpLens_ = lens; }
void Host::setCli(CLI* cli) { cli_ = cli; }



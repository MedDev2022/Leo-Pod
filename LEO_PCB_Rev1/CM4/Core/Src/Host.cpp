#include "Host.hpp"
#include "DayCam.hpp"
#include "LRX20A.hpp"
#include "RPLens.hpp"
#include "IRay.hpp"
#include  "CLI.hpp"
#include <cstdio>
#include "task.h"
#include "comm.hpp"
#include "debug_print.h"
#include "timers.h"

extern UART_HandleTypeDef* g_DebugUart;


Host::Host(UART_HandleTypeDef* huart, uint32_t baudrate)
    : UartEndpoint(huart, "HostTask") {
	baudrate_ = baudrate;

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

    // Set baudrate before starting
    if (huart_->Init.BaudRate != baudrate_) {
        SetBaudrate(baudrate_);
    }

    if (!StartReceive()) {
        printf("Host Start Receive failed\r\n");
    } else {
        printf("Host Start Receive success\r\n");
        rxState_ = RxState::Ready;
    }
}

void Host::timeoutTimerCallback(TimerHandle_t xTimer) {
    Host* host = static_cast<Host*>(pvTimerGetTimerID(xTimer));
    if (host != nullptr) {
        host->handleTimeout();
    } else {
        printf("Host: timeoutTimerCallback - host == nullptr\r\n");
    }
}

// Start the timeout timer (task-context API)
void Host::startTimeoutTimer() {
    printf("Host: startTimeoutTimer called\r\n");
    if (timeoutTimer_ != nullptr) {
        // Clear ignore flag when starting a new timer
        ignoreTimeouts_ = false;

        BaseType_t res = xTimerReset(timeoutTimer_, 0); // 0 = don't block waiting for queue
        if (res != pdPASS) {
            printf("Host: xTimerReset FAILED (res=%ld)\r\n", (long)res);
        } else {
            // Optional: confirm it is active
            UBaseType_t active = xTimerIsTimerActive(timeoutTimer_);
            printf("Host: xTimerReset -> pdPASS, timer active=%u\r\n", (unsigned)active);
        }
    } else {
        printf("Host: startTimeoutTimer called but timeoutTimer_ == nullptr\r\n");
    }
}

//void Host::startTimeoutTimer() {
//    if (timeoutTimer_ != nullptr) {
//        ignoreTimeouts_ = false;
//        // Use task-context version (not FromISR)
//        xTimerReset(timeoutTimer_, 0);  // 0 = don't block if queue full
//    }
//}

//// Stop the timeout timer
//void Host::stopTimeoutTimer() {
//	printf("Host: Stopping timeout timer\r\n");
//    if (timeoutTimer_ != nullptr) {
//        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//        xTimerStopFromISR(timeoutTimer_, &xHigherPriorityTaskWoken);
//        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//    }
//}

// Stop the timeout timer (task-context API)
void Host::stopTimeoutTimer() {
    if (timeoutTimer_ != nullptr) {
        BaseType_t res = xTimerStop(timeoutTimer_, 0);
        if (res != pdPASS) {
            printf("Host: xTimerStop FAILED (res=%ld)\r\n", (long)res);
        } else {
            UBaseType_t active = xTimerIsTimerActive(timeoutTimer_);
            printf("Host: xTimerStop -> pdPASS, timer active=%u\r\n", (unsigned)active);
        }
    } else {
        printf("Host: stopTimeoutTimer called but timeoutTimer_ == nullptr\r\n");
    }
}

//void Host::stopTimeoutTimer() {
//    if (timeoutTimer_ != nullptr) {
//        // Use task-context version (not FromISR)
//        xTimerStop(timeoutTimer_, 0);
//    }
//}

void Host::handleTimeout() {
    // ALWAYS stop timer first
    printf("Host: Timeout occurred\r\n");
    stopTimeoutTimer();

    // Check ignore flag
    if (ignoreTimeouts_) {
        printf("Host: Timeout ignored\r\n");
        resetReception();
        return;
    }

    printf("Host: timeout bufferIndex_=%u rxState=%d\r\n",
           bufferIndex_, (int)rxState_);

    // Check if this is a spurious timeout
    if (rxState_ != RxState::Receiving || bufferIndex_ == 0) {
        printf("Host: Spurious timeout - resetting\r\n");
        resetReception();  // ← CRITICAL: Always reset!
        return;
    }

    // Valid timeout - we have incomplete data
    printf("  Data: ");
    for (uint8_t i = 0; i < bufferIndex_ && i < 16; i++) {
        printf("%02X ", buffer_[i]);
    }
    printf("\r\n");

    // Forward data
    if (destEndpoint_ != nullptr) {
        printf("Host: [Transparent] Timeout - flushing to destination\r\n");
        destEndpoint_->write(buffer_, bufferIndex_);
    } else {
        printf("Host: [Normal] Timeout - discarding incomplete data\r\n");
    }

    // ALWAYS reset at the end
    resetReception();
}
    void Host::processRxData(uint8_t byte) {
        const TickType_t now = xTaskGetTickCount();



        // Handle transparent mode - SNIFF for commands
        if (destEndpoint_  != nullptr) {
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
                    // Buffer overflow - CHANGED: use write()
                    stopTimeoutTimer();
                    printf("Host: [Transparent] Buffer overflow, flushing\r\n");
                    destEndpoint_->write(buffer_, bufferIndex_);  // ← NON-BLOCKING
                    resetReception();
                    destEndpoint_->write(&byte, 1);  // ← NON-BLOCKING
                    return;
                }
                //MK need to make robust for any amount of srcID and dstID
                // ============================================
                // EARLY VALIDATION: Check at byte 2 (SrcID)
                // Valid Host protocol sources: HOST(0x10), LEOPOD(0x70)
                // ============================================
                if (bufferIndex_ == 2) {
                    uint8_t srcID = buffer_[1];
                    if (srcID != HOST_ID ) {
                        // NOT a Host protocol message - forward immediately
                        printf("Host: [Transparent] Not Host protocol (SrcID=0x%02X), forwarding\r\n", srcID);
                        stopTimeoutTimer();
                        destEndpoint_->write(buffer_, bufferIndex_);
                        resetReception();
                        return;
                    }
                }
                // ============================================
                // EARLY VALIDATION: Check at byte 3 (DstID)
                // For commands TO Host: DstID should be HOST(0x10)
                // ============================================
                if (bufferIndex_ == 3) {
                    uint8_t dstID = buffer_[2];
                    if (dstID != LEOPOD_ID && dstID != DAYCAM_ID && dstID != IRAY_ID && dstID != RPLENS_ID && dstID != LRF_ID) {
                        // Not destined for Host - forward immediately
                        printf("Host: [Transparent] Not for Host (DstID=0x%02X), forwarding\r\n", dstID);
                        stopTimeoutTimer();
                        destEndpoint_->write(buffer_, bufferIndex_);
                        resetReception();
                        return;
                    }
                }

                // Parse length when we have it
                if (bufferIndex_ == 6) {
                    expectedLength_ = 6 + buffer_[5] + 2;

                    if (expectedLength_ > sizeof(buffer_)) {
                        // Invalid length - CHANGED: use write()
                        stopTimeoutTimer();
                        printf("Host: [Transparent] Invalid length, flushing\r\n");
                        destEndpoint_->write(buffer_, bufferIndex_);  // ← NON-BLOCKING
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
                        destEndpoint_->write(buffer_, bufferIndex_);  // ← NON-BLOCKING
                        resetReception();
                        return;
                    }

                    // Verify CRC
                    if (!verifyCRC(buffer_, expectedLength_)) {
                        printf("Host: [Transparent] CRC failed, flushing\r\n");
                          destEndpoint_->write(buffer_, bufferIndex_);  // ← NON-BLOCKING
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
                            // Not a control command - forward - CHANGED: use write()
                            printf("Host: [Transparent] Not a control cmd (0x%02X), forwarding\r\n",
                                   rxMsg.m_opCode);
                            destEndpoint_->write(buffer_, bufferIndex_);  // ← NON-BLOCKING
                            resetReception();
                            return;
                        }
                    }
                }

                return;  // Still collecting
            }

            // Normal transparent forwarding - CHANGED: use write()
            destEndpoint_->write(&byte, 1);  // ← NON-BLOCKING
            return;
        }

        else
        {
        // Normal mode (not transparent)
        switch (rxState_) {

        	// Start byte receive timeout timer
        	startTimeoutTimer();

            case RxState::Ready:
                if (byte == 0xAA) {  // Header detected
                    buffer_[0] = byte;
                    bufferIndex_ = 1;
                    firstByteTick_ = now;
                    rxState_ = RxState::Receiving;
                    printf("Host: Header detected, timer started\r\n");
                } else {
                    // Ignore bytes when waiting for header
                    printf("Host: Waiting for header, ignoring 0x%02X\r\n", byte);
                }
                break;

            case RxState::Receiving: {
                // Reset timer on each byte received
  //              startTimeoutTimer();

                // Accumulate byte
                if (bufferIndex_ < sizeof(buffer_)) {
                    buffer_[bufferIndex_++] = byte;
                        printf("Host: Stored byte[%u]=0x%02X\r\n",
                              bufferIndex_-1, byte);
                } else {
                    stopTimeoutTimer();
                    resetReception();
                    printf("Host: Buffer overflow! bufferIndex_=%u\r\n", bufferIndex_);
                    break;
                }

                // Parse length when we have it
                if (bufferIndex_ == 6) {
                    expectedLength_ = 6 + buffer_[5] + 2;  // header(1) + fields(5) + payload + CRC(1) + footer(1)

                    if (expectedLength_ > sizeof(buffer_)) {
                        stopTimeoutTimer();
                        resetReception();
                        printf("Host: Invalid length=%u (payload=%u)\r\n",
                               expectedLength_, buffer_[5]);
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

                        // ========================================================================
                        // MESSAGE ROUTING - Check destination ID
                        // ========================================================================
                        if (rxMsg.m_destID != LEOPOD_ID && rxMsg.m_destID != HOST_ID) {
                            // Message is NOT for us - route it to the correct device
                            printf("Host: Routing message to device 0x%02X\r\n", rxMsg.m_destID);

                            bool forwarded = forwardToDevice(rxMsg);

                            if (forwarded) {
                                printf("Host: Message forwarded to 0x%02X\r\n", rxMsg.m_destID);
                            } else {
                                printf("Host: Failed to forward - unknown/uninitialized device 0x%02X\r\n",
                                       rxMsg.m_destID);
                            }

                            resetReception();
                            break;  // Exit switch - don't process locally
                        }

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
    const uint8_t opCode = msg.getOpCode();

    switch (opCode) {
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

        case 0x21: //RP

            	rpLens_->handleZoomIn();
                printf("RP Lens Zoom In\r\n");

            break;

        case 0x22: //RP
            	rpLens_->handleZoomOut();
                printf("RP Lens Zoom Out\r\n");
            break;
        case 0x23: //RP zoom to position
            if (rpLens_ && payload != nullptr) {
            	rpLens_->SetZoomPosition((payload[1] << 8) | payload[0]);
                printf("RP Lens to position\r\n");
            }
            break;


            	rpLens_->handleZoomOut();
                printf("RP Lens Zoom Out\r\n");
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

        case 0x80:  // Switch device
            if (length == 1 && payload != nullptr) {
                switch (payload[0]) {
                    case 0x01:  // DayCam
                        this->destEndpoint_ = dayCam_;  // ← CHANGED
                        dayCam_->setTransparentMode(true, this);
                        iRay_->setTransparentMode(false, nullptr);
                        lrx20A_->setTransparentMode(false, nullptr);
                        rpLens_->setTransparentMode(false, nullptr);
                        dayCam_->StartReceive();
                        printf("Host switched to DayCam transparent mode\r\n");
                        break;

                    case 0x02:  // IRay
                        this->destEndpoint_ = iRay_;  // ← CHANGED
                        dayCam_->setTransparentMode(false, nullptr);
                        iRay_->setTransparentMode(true, this);
                        lrx20A_->setTransparentMode(false, nullptr);
                        rpLens_->setTransparentMode(false, nullptr);
                        iRay_->StartReceive();
                        printf("Host switched to IRay transparent mode\r\n");
                        break;

                    case 0x03:  // LRF
                        this->destEndpoint_ = lrx20A_;  // ← CHANGED
                        dayCam_->setTransparentMode(false, nullptr);
                        iRay_->setTransparentMode(false, nullptr);
                        lrx20A_->setTransparentMode(true, this);
                        rpLens_->setTransparentMode(false, nullptr);
                        lrx20A_->StartReceive();
                        printf("Host switched to LRF transparent mode\r\n");
                        break;

                    case 0x04:  // RPLens
                        this->destEndpoint_ = rpLens_;  // ← CHANGED
                        dayCam_->setTransparentMode(false, nullptr);
                        iRay_->setTransparentMode(false, nullptr);
                        lrx20A_->setTransparentMode(false, nullptr);
                        rpLens_->setTransparentMode(true, this);
                        rpLens_->StartReceive();
                        printf("Host switched to RPLens transparent mode\r\n");
                        break;

                    case 0x00:  // Exit transparent
                        this->destEndpoint_ = nullptr;  // ← CHANGED
                        dayCam_->setTransparentMode(false, nullptr);
                        iRay_->setTransparentMode(false, nullptr);
                        lrx20A_->setTransparentMode(false, nullptr);
                        rpLens_->setTransparentMode(false, nullptr);
                        printf("Host exit transparent mode\r\n");
                        break;
                }
            }
            break;

        case 0x81:  // Exit transparent mode
            if (length == 0) {
                this->destEndpoint_ = nullptr;  // ← CHANGED
                dayCam_->setTransparentMode(false, nullptr);
                iRay_->setTransparentMode(false, nullptr);
                lrx20A_->setTransparentMode(false, nullptr);
                rpLens_->setTransparentMode(false, nullptr);
                printf("Host exit transparent mode\r\n");
            }
            break;

        case 0x82:  // Debug to Host/CLI
            if (length == 1 && payload != nullptr) {
            	switch (payload[0])
            	{
            	case 0:
            		printf("Debug forwarded to CLI\r\n");
            		g_DebugUart = cli_->huart_;
            		SetDebugOutput(cli_);
            		break;
            	case 1:
            		printf("Debug forwarded to HOST\r\n");
            		g_DebugUart = this->huart_;
            		SetDebugOutput(this);
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

    bufferIndex_ = 0;
    rxState_ = RxState::Ready;
    firstByteTick_ = 0;
    expectedLength_ = 0;
    ignoreTimeouts_ = true;
    printf("Host: resetReception\r\n");

}

bool Host::verifyCRC(uint8_t* msg, size_t len) {
    if (len < 3) return false;

    uint8_t crc = 0x00;
    for (size_t i = 1; i < len - 2; ++i) {
        crc ^= msg[i];
    }


    return crc == msg[len - 2];
}

/**
 * Forward message to the appropriate device based on destination ID
 *
 * @param msg The message to forward
 * @return true if forwarded successfully, false if unknown destination
 */
bool Host::forwardToDevice(const comm::Message& msg) {
    // Rebuild the complete message from parsed data
    uint8_t buffer[256];

    buffer[0] = msg.m_Header;
    buffer[1] = msg.m_srcID;
    buffer[2] = msg.m_destID;
    buffer[3] = msg.m_opCode;
    buffer[4] = msg.m_addr;
    buffer[5] = msg.m_length;

    // Copy payload
    if (msg.m_length > 0) {
        memcpy(buffer + 6, msg.m_payload.data(), msg.m_length);
    }

    buffer[6 + msg.m_length] = msg.m_dataCRC;
    buffer[6 + msg.m_length + 1] = msg.m_Footer;

    size_t totalSize = 8 + msg.m_length;

    // Route to appropriate device based on destination ID
    switch (msg.m_destID) {
        case DAYCAM_ID:  // 0x21
            if (dayCam_ != nullptr) {
                printf("Forwarding to DayCam\r\n");
                dayCam_->write(msg.m_payload.data(), msg.m_length);
                return true;
            }
            printf("DayCam not initialized\r\n");
            break;

        case IRAY_ID:  // 0x22
            if (iRay_ != nullptr) {
                printf("Forwarding to IRay\r\n");
                iRay_->write(msg.m_payload.data(), msg.m_length);
                return true;
            }
            printf("IRay not initialized\r\n");
            break;

        case RPLENS_ID:  // 0x23
            if (rpLens_ != nullptr) {
                printf("Forwarding to RPLens\r\n");
                rpLens_->write(msg.m_payload.data(), msg.m_length);
                return true;
            }
            printf("RPLens not initialized\r\n");
            break;

        case LRF_ID:  // 0x24
            if (lrx20A_ != nullptr) {
                printf("Forwarding to LRF\r\n");
                lrx20A_->write(msg.m_payload.data(), msg.m_length);
                return true;
            }
            printf("LRF not initialized\r\n");
            break;

        default:
            printf("Unknown device ID: 0x%02X\r\n", msg.m_destID);
            return false;
    }

    // Device pointer was null
    return false;
}

void Host::setDayCam(DayCam* cam) { dayCam_ = cam; }
void Host::setLRF(LRX20A* lrf) { lrx20A_ = lrf; }
void Host::setIRay(IRay* cam) { iRay_ = cam; }
void Host::setRPLens(RPLens* lens) { rpLens_ = lens; }
void Host::setCli(CLI* cli) { cli_ = cli; }



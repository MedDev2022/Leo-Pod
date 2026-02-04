#include "Host.hpp"
#include "DayCam.hpp"
#include "LRX20A.hpp"
#include "RPLens.hpp"
#include "IRay.hpp"
#include  "CLI.hpp"
#include <cstdio>
#include "task.h"
#include "comm.hpp"
#include "timers.h"
#include "debug_printf.hpp"

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


    if (dayCam_)  dayCam_->destEndpointW_ = this;
    if (iRay_)    iRay_->destEndpointW_ = this;
    if (lrx20A_)  lrx20A_->destEndpointW_ = this;
    if (rpLens_)  rpLens_->destEndpointW_ = this;

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
    void Host::processRxData(const uint8_t* data, uint16_t length) {

        if (data == nullptr || length == 0 ) {
            return;
        }

        // ========================================
        // Use comm::Message::parse() - handles:
        //   - Header/footer validation
        //   - Length validation
        //   - CRC verification
        //   - Payload extraction
        // ========================================
        comm::Message rxMsg;

        if (!rxMsg.parse(data, length)) {
            printf("Host: Message parse failed (invalid format or CRC)\r\n");
            return;
        }

        printf("Host: Valid! Src=0x%02X Dest=0x%02X Op=0x%02X Len=%u\r\n",
               rxMsg.m_srcID, rxMsg.m_destID, rxMsg.m_opCode, rxMsg.m_length);

        // ========================================
        // ROUTE based on destination
        // ========================================
        if (rxMsg.m_destID != LEOPOD_ID && rxMsg.m_destID != HOST_ID) {
            // Message is NOT for us - forward PAYLOAD ONLY to target device
//            printf("Host: Routing to device 0x%02X (%s)\r\n",
//                   rxMsg.m_destID, comm::getDeviceName(rxMsg.m_destID));

            if (forwardPayloadToDevice(rxMsg)) {
               // printf("Host: Payload forwarded successfully\r\n");
            } else {
               // printf("Host: Forward failed - unknown device 0x%02X\r\n", rxMsg.m_destID);
            }
            return;
        }

        // ========================================
        // PROCESS locally (message for Host/Leopod)
        // ========================================
        parseAndProcess(rxMsg);
    }
    // ============================================================================
    // FORWARD PAYLOAD ONLY TO DEVICE
    // Extracts payload from message and sends raw bytes to target device
    // ============================================================================


//void Host::processRxData(const uint8_t* data, uint16_t length) {
//    if (data == nullptr || length == 0) {
//        return;
//    }
//
//    uint16_t offset = 0;
//
//    while (offset < length) {
//        // Skip until we find a header byte
//        while (offset < length && data[offset] != comm::HEADER_BYTE) {
//            offset++;
//        }
//
//        // No more potential messages
//        if (offset >= length) {
//            break;
//        }
//
//        // Check if enough bytes remain for minimum message
//        size_t remaining = length - offset;
//        if (remaining < 6 + 2) {  // header + CRC + footer minimum
//            break;
//        }
//
//        // Try to parse
//        comm::Message rxMsg;
//        uint16_t consumed = rxMsg.parse(data + offset, remaining);
//
//        if (consumed > 0) {
//            // Valid message - process it
//            offset += consumed;
//
//            if (rxMsg.m_destID != LEOPOD_ID && rxMsg.m_destID != HOST_ID) {
//                forwardPayloadToDevice(rxMsg);
//            } else {
//                parseAndProcess(rxMsg);
//            }
//        } else {
//            // Invalid message at this header byte - skip it and search for next
//            offset++;
//        }
//    }
//}

bool Host::forwardPayloadToDevice(const comm::Message& msg) {

        UartEndpoint* targetDevice = nullptr;

        // Select target device based on destination ID
        switch (msg.m_destID) {
            case DAYCAM_ID:  // 0x21
                targetDevice = dayCam_;
                break;

            case IRAY_ID:    // 0x22
                targetDevice = iRay_;
                break;

            case RPLENS_ID:  // 0x23
                targetDevice = rpLens_;
                break;

            case LRF_ID:     // 0x24
                targetDevice = lrx20A_;
                break;

            default:
                printf("Host: Unknown device ID: 0x%02X\r\n", msg.m_destID);
                return false;
        }

        if (targetDevice == nullptr) {
            printf("Host: Device 0x%02X not initialized\r\n", msg.m_destID);
            return false;
        }

        // Send PAYLOAD ONLY to the device
          if (msg.m_length > 0 && !msg.m_payload.empty()) {
              // Copy payload for decryption (don't modify original message)
              uint8_t decrypted[256];
              uint16_t copyLen = (msg.m_length > 256) ? 256 : msg.m_length;
              memcpy(decrypted, msg.m_payload.data(), copyLen);

              // Decrypt the payload before sending to device
              DecryptInPlace(decrypted, copyLen);

//              printf("Host: Decrypted %u bytes, sending to %s\r\n",
//                     copyLen, comm::getDeviceName(msg.m_destID));

              // Debug: show decrypted data
              //printf("  Decrypted: ");
              for (uint16_t i = 0; i < copyLen; i++) {
                  //printf("%02X ", decrypted[i]);
              }
              //printf("\r\n");

              targetDevice->SendCommand(decrypted, copyLen);
              return true;
          }

        //  printf("Host: No payload to forward\r\n");
          return false;
    }

// ============================================================================
// SEND DEVICE RESPONSE BACK TO EXTERNAL CONTROLLER
// Called by devices when they receive data from hardware
// Encrypts payload and wraps in proper message format with CRC
// ============================================================================
void Host::sendDeviceResponse(uint8_t sourceDeviceID,
                              const uint8_t* data,
                              uint16_t length,
                              uint8_t opCode,
                              uint8_t addr) {


    if (data == nullptr || length == 0) {
        printf("Host: sendDeviceResponse - no data\r\n");
        return;
    }

    if (length > 255) {  // Fixed: was comm::MAX_PAYLOAD_SIZE which overflowed to 0
        printf("Host: Response too large: %u bytes (max 255)\r\n", length);
        return;
    }

    // Copy and encrypt the payload
    uint8_t encrypted[256];
    memcpy(encrypted, data, length);
    EncryptInPlace(encrypted, length);

    // Debug: show original and encrypted
    printf("Host: Encrypting response from 0x%02X (%s)\r\n",
           sourceDeviceID, comm::getDeviceName(sourceDeviceID));
    printf("  Plain:     ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");
    printf("  Encrypted: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", encrypted[i]);
    }
    printf("\r\n");

    // Build response message
    // Source = the device that received the response
    // Destination = HOST (external controller)
    comm::Message response;
    response.m_Header  = comm::HEADER_BYTE;
    response.m_srcID   = sourceDeviceID;
    response.m_destID  = HOST_ID;  // Send to external controller
    response.m_opCode  = opCode;
    response.m_addr    = addr;
    response.m_length  = static_cast<uint8_t>(length);
    response.setPayload(encrypted, length);  // Use encrypted payload
    response.updateCRC();
    response.m_Footer  = comm::FOOTER_BYTE;

    // Build raw buffer
    std::vector<uint8_t> buffer = response.build();

    printf("  Msg:       ");
    for (size_t i = 0; i < buffer.size(); i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\r\n");

    // Send to external controller via Host UART
    this->write(buffer.data(), buffer.size());
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
        case 0x24: //RP focus in
				//rpLens_->handleFocusIn();
        		rpLens_->SetFocusIncrement(100);
				printf("RP Lens Focus in\r\n");
			break;
        case 0x25: //RP focus out
				//rpLens_->handleFocusOut();
				rpLens_->SetFocusDecrement(100);
				printf("RP Lens Focus Out\r\n");
			break;

        case 0x26:  // RPLens Autofocus Request
            if (rpLens_) {
                //rpLens_->handleAutofocus();
                //rpLens_->handleQuickAutofocus();
                rpLens_->handleSmoothAutofocus();
                DBG_INFO("RP Lens Autofocus\r\n");
            }
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

        case 0x82:  // Debug output routing
            if (length == 1 && payload != nullptr) {
                switch (payload[0]) {
                    case 0:
                        DebugPrintf_SetUart(cli_->huart_);
                        DBG_INFO("Debug to CLI");
                        break;
                    case 1:
                        DebugPrintf_SetUart(this->huart_);
                        DBG_INFO("Debug to HOST");
                        break;
                    case 0xFF:
                        DebugPrintf_Enable(0);  // Disable debug output
                        break;
                }
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
	return 0;
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



void Host::setDayCam(DayCam* cam) { dayCam_ = cam; if (dayCam_) dayCam_->destEndpointW_ = this; }
void Host::setLRF(LRX20A* lrf) { lrx20A_ = lrf; }
void Host::setIRay(IRay* cam) { iRay_ = cam; }
void Host::setRPLens(RPLens* lens) { rpLens_ = lens; }
void Host::setCli(CLI* cli) { cli_ = cli; }



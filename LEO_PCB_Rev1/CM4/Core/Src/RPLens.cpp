
#include <RPLens.hpp>
#include "Host.hpp"
#include <cstring>
#include <queue>
#include "cmsis_os.h"
#include "comm.hpp"
#include "debug_printf.hpp"

// Shared mailbox with M7 core
#define SHARED_MAILBOX_ADDR  0x30010000
#define XY_SAMPLES           64

typedef struct __attribute__((aligned(32))) {
    /* Request (M4 -> M7) */
    volatile uint32_t req_cmd;      /* 0=idle, 1=capture */
    volatile uint32_t req_step;     /* Current focus step 0-9 */
    volatile uint32_t req_seq;      /* Sequence number */
    uint32_t _pad1[5];              /* Pad to 32 bytes */

    /* Response (M7 -> M4) */
    volatile uint32_t rsp_ready;    /* 1 = data ready */
    volatile uint32_t rsp_seq;      /* Echo of req_seq */
    volatile uint32_t rsp_step;     /* Echo of req_step */
    volatile uint32_t rsp_status;   /* 0 = OK, non-zero = error */
    volatile uint64_t  sumX;         /* Sum of all X values */
    volatile uint64_t  sumY;         /* Sum of all Y values */

    /* Data (M7 -> M4) */
    volatile uint32_t x[XY_SAMPLES]; /* 64 X values */
    volatile uint32_t y[XY_SAMPLES]; /* 64 Y values */

} SharedMailbox_t;

#define g_mb  (*((volatile SharedMailbox_t*)SHARED_MAILBOX_ADDR))

// Mailbox commands
#define MB_IDLE         0
#define MB_REQ_CAPTURE  1

//RPLens::RPLens(USART_TypeDef * portName, int defaultBaudRate)
//   : UartEndpoint(portName, defaultBaudRate, SERIAL_8N1) {}

RPLens::RPLens(UART_HandleTypeDef* huart, uint32_t baudrate)
    : UartEndpoint(huart, "RPLensTask") {
	baudrate_ = baudrate;
}  // Remove space from task name



//bool RPLens::Connect(USART_TypeDef * portName, int baudRate) {
//    bool isConnected = UartEndpoint::Connect(portName, baudRate);
//    if (isConnected) {
//        ProcessData();
//    }
//    return isConnected;
//}

void RPLens::Init() {
    // Set baudrate before starting
    if (huart_->Init.BaudRate != baudrate_) {
        SetBaudrate(baudrate_);
    }

    if (!StartReceive()) {
        printf("RPLens Start Receive failed\n");
    } else {
        printf("RPLens Start Receive success\n");
    }
}


// ============================================================================
// PROCESS DATA RECEIVED FROM RPLENS MOTOR CONTROLLER
// Encrypt and send back to Host for forwarding to external controller
// ============================================================================
void RPLens::processRxData(const uint8_t* data, uint16_t length) {

    if (data == nullptr || length == 0) {
        return;
    }

    // Debug: Print raw received data
    printf("RPLens RX: %u bytes: ", length);
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");

    // Handle transparent mode - forward raw data
    if (commMode_ == DevCommMode::Transparent && destEndpoint_ != nullptr) {
        destEndpoint_->write(data, length);
        return;
    }

    // Normal mode - send plain data to Host (Host will encrypt)
    if (destEndpointW_ != nullptr) {
        printf("RPLens: Sending response to Host\r\n");
        Host* host = static_cast<Host*>(destEndpointW_);
        host->sendDeviceResponse(RPLENS_ID, data, length);  // Plain data - Host encrypts
    } else {
        printf("RPLens: No Host endpoint configured\r\n");
    }
}

void RPLens::ZoomStop() {
    std::vector<uint8_t> cmd = {0x24, 0x14, 0x00, 0x00, 0x30};
    UpdateCRC(cmd);
    SendCommand(cmd.data(), cmd.size());
}

void RPLens::ZoomIn() {
    std::vector<uint8_t> cmd = {0x24, 0x23, 0x00, 0x00, 0x07};

    const uint8_t command[] = {0x30, 0x01, 0x00};

    UpdateCRC(cmd);
//    StartReceive();  // Start receiving in preparation
    SendCommand(cmd.data(), cmd.size());
}


void RPLens::handleZoomIn(void){

    uint8_t cmd[] = {0x24, 0x23, 0x00, 0x00, 0x07};
	SendCommand(cmd, sizeof(cmd));

}

void RPLens::handleZoomOut(void){

    uint8_t cmd[] = {0x24, 0x24, 0x00, 0x00, 0x00};
	SendCommand(cmd, sizeof(cmd));

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
    std::vector<uint8_t> cmd = {0x24, 0x47, 0x02, 0x00,  static_cast<uint8_t>(pos & 0xFF), static_cast<uint8_t>((pos >> 8) & 0xFF), 0x00};
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
	std::vector<uint8_t> cmd = {0x24, 0x0A, 0x0, 0x0, 0x2E };
	UpdateCRC(cmd);
	SendCommand(cmd.data(), cmd.size());
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



void processIncoming() {

}

void RPLens::handleAutofocus() {
    DBG_INFO("RPLens: Starting Autofocus");

    // Clear previous results
    memset(afResults_, 0, sizeof(afResults_));

    // Reset to starting position first
    SetFastFocusPosition(AF_START_POSITION);
    osDelay(AF_MOTOR_SETTLE_MS);

    // Run the focus scan
    runFocusScan();

    // Find best position from results
    int8_t bestStep = -1;
    int64_t bestScore = 0;

    for (uint8_t step = 0; step < AF_NUM_STEPS; step++) {
        if (afResults_[step].valid) {
            int64_t absScore = afResults_[step].totalScore;
            if (absScore < 0) absScore = -absScore;

            if (bestStep < 0 || absScore > bestScore) {
                bestScore = absScore;
                bestStep = step;
            }
        }
    }

    // Move to best focus position
    if (bestStep >= 0) {
        DBG_INFO("RPLens: Best focus at step %d, position %u, score %lld",
                 bestStep, afResults_[bestStep].focusPosition, (long long)bestScore);
        SetFastFocusPosition(afResults_[bestStep].focusPosition);
    } else {
        DBG_ERR("RPLens: Autofocus failed - no valid results");
    }
}

void RPLens::printRawXYData() {
    DBG_INFO("=== RAW X/Y DATA ===");
    DBG_INFO("  sumX: %lld", (long long)g_mb.sumX);
    DBG_INFO("  sumY: %lld", (long long)g_mb.sumY);

    // If mailbox has raw arrays (check your M7 struct definition)
    // Uncomment if g_mb has x[] and y[] arrays:

    for (int i = 0; i < 64; i++) {  // Adjust size as needed
        DBG_INFO("%d,%lu,%lu", i, (unsigned long)g_mb.x[i], (unsigned long)g_mb.y[i]);
    }
    DBG_INFO("====================");

}

void RPLens::runFocusScan() {
    DBG_INFO("RPLens: Running focus scan...");

    uint32_t seq = 0;

    // Clear results
    memset(afResults_, 0, sizeof(afResults_));

    // Move to start position
    DBG_INFO("  Moving to start position %d...", AF_START_POSITION);
    SetFastFocusPosition(AF_START_POSITION);
    osDelay(500);

    // Clear mailbox request
    g_mb.req_cmd = MB_IDLE;
    g_mb.req_seq = 0;
    __DMB();
    osDelay(100);

    // Run scan
    for (uint8_t step = 0; step < AF_NUM_STEPS; step++) {
        uint16_t currentPosition = AF_START_POSITION + (step * AF_STEP_INCREMENT);

        // Move focus (skip for step 0)
        if (step > 0) {
        	SetFastFocusPosition(currentPosition);
            osDelay(AF_MOTOR_SETTLE_MS);
        }

        // Store position
        afResults_[step].focusPosition = currentPosition;

        // Request capture
        seq++;

        // Clear response flag
        g_mb.rsp_ready = 0;

        // Set request parameters
        g_mb.req_step = step;
        g_mb.req_seq = seq;
        __DMB();

        // Send request command
        g_mb.req_cmd = MB_REQ_CAPTURE;
        __DMB();

        // Wait for response with timeout (matching old RequestCapture)
        uint32_t startTime = osKernelGetTickCount();
        bool success = false;

        while (1) {
            uint32_t now = osKernelGetTickCount();

            // Check timeout
            if ((now - startTime) > AF_CAPTURE_TIMEOUT_MS) {
                break;
            }

            // Memory barrier before reading
            __DMB();

            // Check if response ready with matching sequence (NOT checking rsp_step!)
            if (g_mb.rsp_ready == 1 && g_mb.rsp_seq == seq) {
                success = true;
                break;
            }
        }

        if (success) {
            __DMB();
            afResults_[step].sumX = g_mb.sumX;
            afResults_[step].sumY = g_mb.sumY;
            afResults_[step].totalScore = g_mb.sumX + g_mb.sumY;
            afResults_[step].valid = 1;

            DBG_INFO("  Step %d: pos=%u, score=%lld",
                   step, afResults_[step].focusPosition,
                   (long long)afResults_[step].totalScore);
        } else {
            afResults_[step].valid = 0;
            DBG_WARN("  Step %d: FAILED", step);
        }

        // Print raw data for debugging
       // printRawXYData();


        g_mb.req_cmd = MB_IDLE;
        __DMB();
    }

   // printAutofocusResults();
}



void RPLens::printAutofocusResults() {
    DBG_INFO("=== AUTOFOCUS RESULTS ===");

    int8_t bestStep = -1;
    int64_t bestScore = 0;

    for (uint8_t step = 0; step < AF_NUM_STEPS; step++) {
        if (afResults_[step].valid) {
            DBG_INFO("Step %2d: Pos=%4u, Score=%lld",
                   step,
                   afResults_[step].focusPosition,
                   (long long)afResults_[step].totalScore);

            int64_t absScore = afResults_[step].totalScore;
            if (absScore < 0) absScore = -absScore;

            if (bestStep < 0 || absScore > bestScore) {
                bestScore = absScore;
                bestStep = step;
            }
        }
    }

    if (bestStep >= 0) {
        DBG_INFO(">>> BEST: Step %d, Pos %u <<<",
               bestStep, afResults_[bestStep].focusPosition);
    }
    DBG_INFO("=========================");
}



#include "UartEndpoint.hpp"
#include <cstdio>
#include <cstring>
#include "task.h"


#include "debug_print.h"




std::map<UART_HandleTypeDef*, UartEndpoint*> UartEndpoint::instanceMap;

UartEndpoint::UartEndpoint(UART_HandleTypeDef* huart, const char* taskName)
	: huart_(huart),
	  taskName_(taskName),
	  taskHandle_(nullptr),
	  txBusy_(false),
	  txPending_(false) {


	instanceMap[huart] = this;

    // Create RX message queue (64 bytes deep)
    const osMessageQueueAttr_t queueAttr = {
        .name = "UartRxQueue"
    };
    rxQueue_ = osMessageQueueNew(64, sizeof(uint8_t), &queueAttr);

    if (rxQueue_ == nullptr) {
        printf("Failed to create RX queue for %s\r\n", taskName_);
        return;
    }

    // Create TX message queue (2048 bytes deep)
    const osMessageQueueAttr_t txQueueAttr = {
        .name = "UartTxQueue"
    };
    txQueue_ = osMessageQueueNew(TX_QUEUE_SIZE, sizeof(uint8_t), &txQueueAttr);

    if (txQueue_ == nullptr) {
        printf("Failed to create TX queue for %s\r\n", taskName_);
        return;
    }

    // Create the RX task
    const osThreadAttr_t taskAttr = {
        .name = taskName_,
        .stack_size = 2048,  // Adjust as needed
        .priority = osPriorityNormal
    };
    taskHandle_ = osThreadNew(TaskEntry, this, &taskAttr);

    if (taskHandle_ == nullptr) {
        printf("Failed to create task for %s\r\n", taskName_);
    }
}


UartEndpoint::~UartEndpoint() {
    if (taskHandle_) {
        osThreadTerminate(taskHandle_);
    }
    if (rxQueue_) {
        osMessageQueueDelete(rxQueue_);
    }
    if (txQueue_) {
        osMessageQueueDelete(txQueue_);
    }
    instanceMap.erase(huart_);
}

bool UartEndpoint::SetBaudrate(uint32_t baudrate) {
    if (huart_ == nullptr) return false;

    // Abort any pending transfers
    HAL_UART_AbortTransmit(huart_);
    HAL_UART_AbortReceive(huart_);

    // Store new baudrate
    huart_->Init.BaudRate = baudrate;

    // Reinitialize
    return (HAL_UART_Init(huart_) == HAL_OK);
}

bool UartEndpoint::StartReceive() {
    return HAL_UART_Receive_IT(huart_, &rxByte_, 1) == HAL_OK;
}

uint16_t UartEndpoint::SendCommand(const uint8_t* command, size_t length) {
    return write(command, length);
}


//// ============================================================
//// NON-BLOCKING PRINTF IMPLEMENTATION
//// ============================================================
//
//int UartEndpoint::printf(const char* format, ...) {
//    char buffer[256];  // Temporary buffer for formatted string
//
//    va_list args;
//    va_start(args, format);
//    int length = vsnprintf(buffer, sizeof(buffer), format, args);
//    va_end(args);
//
//    if (length < 0) {
//        return -1;  // Formatting error
//    }
//
//    // Truncate if too long
//    if (length >= (int)sizeof(buffer)) {
//        length = sizeof(buffer) - 1;
//    }
//
//    // Write to TX queue
//   return write((uint8_t*)buffer, length);
//
//   // return  g_debugOutput->write((uint8_t*)buffer, length);
//
//
//}

int UartEndpoint::write(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return 0;
    }

    size_t written = 0;

    // Put all bytes into TX queue (non-blocking)
    for (size_t i = 0; i < length; i++) {
        // Use timeout=0 for non-blocking
        if (osMessageQueuePut(txQueue_, &data[i], 0, 0) == osOK) {
            written++;
        } else {
            // Queue full, stop trying
            break;
        }
    }

    // Kick off transmission if not already running
    if (written > 0) {
        // Use critical section to safely check and start TX
        UBaseType_t savedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

        if (!txBusy_) {
            // Try to get first byte and start transmission
            if (osMessageQueueGet(txQueue_, &txByte_, nullptr, 0) == osOK) {
                txBusy_ = true;
                taskEXIT_CRITICAL_FROM_ISR(savedInterruptStatus);

                HAL_StatusTypeDef status = HAL_UART_Transmit_IT(huart_, &txByte_, 1);
                if (status != HAL_OK) {
                    // Failed to start - put byte back and clear flag
                    txBusy_ = false;
                }
            } else {
                taskEXIT_CRITICAL_FROM_ISR(savedInterruptStatus);
            }
        } else {
            taskEXIT_CRITICAL_FROM_ISR(savedInterruptStatus);
        }
    }

    return (int)written;
}

uint32_t UartEndpoint::getTxSpace() const {
    if (txQueue_ == nullptr) return 0;
    return osMessageQueueGetSpace(txQueue_);
}

void UartEndpoint::getTxStats(uint32_t& used, uint32_t& capacity) const {
    capacity = TX_QUEUE_SIZE;
    if (txQueue_ != nullptr) {
        used = osMessageQueueGetCount(txQueue_);
    } else {
        used = 0;
    }
}

// ============================================================
// TX TRANSMISSION
// ============================================================
void UartEndpoint::startNextTransmission() {

    if (txBusy_) {
        return;  // Already transmitting
    }

    // Try to get next byte from queue
    if (osMessageQueueGet(txQueue_, &txByte_, nullptr, 0) == osOK) {
        // Mark as busy before starting transmission
        txBusy_ = true;

        // Start transmission of single byte
        HAL_StatusTypeDef status = HAL_UART_Transmit_IT(huart_, &txByte_, 1);

        if (status != HAL_OK) {
            // Failed to start transmission, mark as not busy
            txBusy_ = false;
        }
    }
}

void UartEndpoint::setTransparentMode(bool enable, UartEndpoint* destination) {
    if (enable && destination) {
        commMode_ = DevCommMode::Transparent;
        destEndpoint_ = destination;
    } else {
        commMode_ = DevCommMode::Normal;
        destEndpoint_ = nullptr;
    }
}


// ============================================================
// RX TASK
// ============================================================

void UartEndpoint::TaskEntry(void* argument) {
    UartEndpoint* instance = static_cast<UartEndpoint*>(argument);
    if (instance) {
        instance->taskLoop();
    }
}

void UartEndpoint::taskLoop() {
    uint8_t byte;

    while (true) {
        // Block waiting for at least ONE byte
        osStatus_t status = osMessageQueueGet(rxQueue_, &byte, nullptr, osWaitForever);

        if (status == osOK) {
            // Handle transparent mode
            if (commMode_ == DevCommMode::Transparent && destEndpoint_  != nullptr) {
                destEndpoint_->write(&byte, 1);
                // Continue to drain queue in transparent mode
                while (osMessageQueueGet(rxQueue_, &byte, nullptr, 0) == osOK) {
                	destEndpoint_->write(&byte, 1);
                }
                continue;
            }

            // Pass byte to derived class for processing
            processRxData(byte);
        }
    }
}

// ============================================================
// ISR CALLBACKS
// ============================================================

void UartEndpoint::DispatchTxComplete(UART_HandleTypeDef* huart) {
    auto it = instanceMap.find(huart);
    if (it != instanceMap.end()) {
        UartEndpoint* instance = it->second;
        if (instance) {
            // Clear busy flag FIRST
            instance->txBusy_ = false;

            // Immediately try next byte while still in ISR
            // This keeps the UART fed without gaps
            uint8_t nextByte;
            if (osMessageQueueGet(instance->txQueue_, &nextByte, nullptr, 0) == osOK) {
                instance->txByte_ = nextByte;
                instance->txBusy_ = true;

                if (HAL_UART_Transmit_IT(instance->huart_, &instance->txByte_, 1) != HAL_OK) {
                    instance->txBusy_ = false;
                }
            }
        }
    }
}

void UartEndpoint::DispatchRxComplete(UART_HandleTypeDef* huart) {
    auto it = instanceMap.find(huart);
    if (it != instanceMap.end()) {
        UartEndpoint* instance = it->second;
        if (instance) {
            uint8_t byte = instance->rxByte_;
            osMessageQueuePut(instance->rxQueue_, &byte, 0, 0);
            HAL_UART_Receive_IT(instance->huart_, &instance->rxByte_, 1);
        }
    }
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    UartEndpoint::DispatchRxComplete(huart);
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    UartEndpoint::DispatchTxComplete(huart);
}


bool UartEndpoint::testBlockingTx(const uint8_t* data, size_t length) {
    HAL_StatusTypeDef status = HAL_UART_Transmit(huart_, (uint8_t*)data, length, 1000);
    return (status == HAL_OK);
}

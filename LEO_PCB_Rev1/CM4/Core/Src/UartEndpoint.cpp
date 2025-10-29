#include "UartEndpoint.hpp"
#include <cstdio>
#include "task.h"

std::map<UART_HandleTypeDef*, UartEndpoint*> UartEndpoint::instanceMap;

UartEndpoint::UartEndpoint(UART_HandleTypeDef* huart, const char* taskName)
    : huart_(huart), taskName_(taskName), taskHandle_(nullptr) {

    instanceMap[huart] = this;

    // Create message queue (64 bytes deep)
    const osMessageQueueAttr_t queueAttr = {
        .name = "UartRxQueue"
    };
    rxQueue_ = osMessageQueueNew(64, sizeof(uint8_t), &queueAttr);

    if (rxQueue_ == nullptr) {
        printf("Failed to create RX queue for %s\r\n", taskName_);
        return;
    }

    // Create the task
    const osThreadAttr_t taskAttr = {
        .name = taskName_,
        .stack_size = 2048,  // Adjust as needed
        .priority = osPriorityAboveNormal
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
    instanceMap.erase(huart_);
}

bool UartEndpoint::StartReceive() {
    return HAL_UART_Receive_IT(huart_, &rxByte_, 1) == HAL_OK;
}

uint16_t UartEndpoint::SendCommand(const uint8_t* command, size_t length) {
    if (HAL_UART_Transmit(huart_, command, length, 500) == HAL_OK)
        return length;
    return 0;
}

void UartEndpoint::setTransparentMode(bool enable, UART_HandleTypeDef* destination) {
    if (enable && destination) {
        commMode_ = DevCommMode::Transparent;
        destHuart_ = destination;
    } else {
        commMode_ = DevCommMode::Normal;
        destHuart_ = nullptr;
    }
}

// Static task entry point
void UartEndpoint::TaskEntry(void* argument) {
    UartEndpoint* instance = static_cast<UartEndpoint*>(argument);
    if (instance) {
        instance->taskLoop();
    }
}



// Virtual task loop - can be overridden by derived classes
void UartEndpoint::taskLoop() {
    uint8_t byte;

    while (true) {
        // Block waiting for at least ONE byte
        osStatus_t status = osMessageQueueGet(rxQueue_, &byte, nullptr, osWaitForever);


        if (status == osOK) {
            // Handle transparent mode
            if (commMode_ == DevCommMode::Transparent && destHuart_ != nullptr) {
                HAL_UART_Transmit(destHuart_, &byte, 1, 100);
                // Continue to drain queue in transparent mode
                while (osMessageQueueGet(rxQueue_, &byte, nullptr, 0) == osOK) {
                    HAL_UART_Transmit(destHuart_, &byte, 1, 100);
                }
                continue;
            }

            // Pass the byte we just got to processRxData
            // It will drain the rest of the queue
            processRxData(byte);

//            uint32_t count = osMessageQueueGetCount(rxQueue_);
//            uint32_t capacity = osMessageQueueGetCapacity(rxQueue_);
//            uint32_t space = osMessageQueueGetSpace(rxQueue_);
//
//            printf("[%s] Queue: %lu/%lu used (%lu free) - %.1f%% full\r\n",
//                   taskName_,
//                   count,
//                   capacity,
//                   space,
//                   (float)count * 100.0f / capacity);
        }
    }
}

// ISR callback - ONLY pushes byte to queue
void UartEndpoint::DispatchRxComplete(UART_HandleTypeDef* huart) {

    auto it = instanceMap.find(huart);
    if (it != instanceMap.end()) {
        UartEndpoint* instance = it->second;
        if (instance) {
            uint8_t byte = instance->rxByte_;

            // Check if queue is full before pushing
            uint32_t space = osMessageQueueGetSpace(instance->rxQueue_);
            if (space == 0) {
                // Queue full! Data loss imminent
                // Could set a flag here to notify task
                static uint32_t overflowCount = 0;
                overflowCount++;
                // Note: Can't printf in ISR, but could toggle LED or set flag
            }

            // Push to queue (will fail if full, but we try anyway)

            osStatus_t status = osMessageQueuePut(instance->rxQueue_, &byte, 0, 0);

//
//            if (status != osOK) {
//                // Failed to push - queue is full
//                // Set error flag that task can check
//            }

            // Re-arm UART
            HAL_UART_Receive_IT(instance->huart_, &instance->rxByte_, 1);
        }
    }
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    UartEndpoint::DispatchRxComplete(huart);
}

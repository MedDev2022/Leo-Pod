#include "UartEndpoint.hpp"
#include <cstdio>
#include <cstring>
#include "task.h"

std::map<UART_HandleTypeDef*, UartEndpoint*> UartEndpoint::instanceMap;

UartEndpoint::UartEndpoint(UART_HandleTypeDef* huart, const char* taskName)
    : huart_(huart),
      rxStream_(nullptr),
      txStream_(nullptr),
      rxTaskHandle_(nullptr),
      txTaskHandle_(nullptr),
      taskName_(taskName)
{
    instanceMap[huart] = this;

    // Create stream buffers
    rxStream_ = xStreamBufferCreate(RX_BUFFER_SIZE, 1);
    txStream_ = xStreamBufferCreate(TX_BUFFER_SIZE, 1);

    // Create RX task
    char rxName[configMAX_TASK_NAME_LEN];
    snprintf(rxName, sizeof(rxName), "%.6s_RX", taskName);
    const osThreadAttr_t rxAttr = {
        .name = rxName,
        .stack_size = 2048,
        .priority = osPriorityAboveNormal
    };
    rxTaskHandle_ = osThreadNew(RxTaskEntry, this, &rxAttr);

    // Create TX task
    char txName[configMAX_TASK_NAME_LEN];
    snprintf(txName, sizeof(txName), "%.6s_TX", taskName);
    const osThreadAttr_t txAttr = {
        .name = txName,
        .stack_size = 1024,
        .priority = osPriorityNormal
    };
    txTaskHandle_ = osThreadNew(TxTaskEntry, this, &txAttr);
}

UartEndpoint::~UartEndpoint() {
    if (rxTaskHandle_) osThreadTerminate(rxTaskHandle_);
    if (txTaskHandle_) osThreadTerminate(txTaskHandle_);
    if (rxStream_) vStreamBufferDelete(rxStream_);
    if (txStream_) vStreamBufferDelete(txStream_);
    instanceMap.erase(huart_);
}

bool UartEndpoint::SetBaudrate(uint32_t baudrate) {
    if (huart_ == nullptr) return false;
    HAL_UART_AbortTransmit(huart_);
    HAL_UART_AbortReceive(huart_);
    huart_->Init.BaudRate = baudrate;
    return (HAL_UART_Init(huart_) == HAL_OK);
}

bool UartEndpoint::StartReceive() {
    return HAL_UARTEx_ReceiveToIdle_IT(huart_, rxBuffer_, RX_BUFFER_SIZE) == HAL_OK;
}

bool UartEndpoint::ReStartReceive() {
    HAL_UART_AbortTransmit(huart_);
    HAL_UART_AbortReceive(huart_);
    return HAL_UARTEx_ReceiveToIdle_IT(huart_, rxBuffer_, RX_BUFFER_SIZE) == HAL_OK;
}

uint16_t UartEndpoint::SendCommand(const uint8_t* command, size_t length) {
    return write(command, length);
}

// ============================================================
// WRITE - Non-blocking, appends to TX stream
// Can be called from any task or from another endpoint
// ============================================================
int UartEndpoint::write(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return 0;
    }

    size_t written = xStreamBufferSend(txStream_, data, length, 0);

    // Wake TX task if it's waiting - cast to TaskHandle_t
    if (written > 0 && txTaskHandle_ != nullptr) {
        xTaskNotifyGive((TaskHandle_t)txTaskHandle_);
    }

    return (int)written;
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
// TASK ENTRY POINTS
// ============================================================

void UartEndpoint::RxTaskEntry(void* arg) {
    static_cast<UartEndpoint*>(arg)->rxTaskLoop();
}

void UartEndpoint::TxTaskEntry(void* arg) {
    static_cast<UartEndpoint*>(arg)->txTaskLoop();
}

// ============================================================
// RX TASK - Processes data received by ISR
// ============================================================
void UartEndpoint::rxTaskLoop() {
    uint8_t buffer[RX_BUFFER_SIZE];

    while (true) {
        // Block until data available from ISR
        size_t count = xStreamBufferReceive(rxStream_, buffer, RX_BUFFER_SIZE, portMAX_DELAY);

        if (count > 0) {
            if (commMode_ == DevCommMode::Transparent && destEndpoint_ != nullptr) {
                destEndpoint_->write(buffer, count);
            } else {
                processRxData(buffer, count);
            }
        }
    }
}

// ============================================================
// TX TASK - Transmits data using interrupts (non-blocking)
// ============================================================
void UartEndpoint::txTaskLoop() {
    while (true) {
        // Wait for notification that data is available
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Drain the TX stream buffer
        while (true) {
            // Get chunk from stream buffer (non-blocking)
            size_t count = xStreamBufferReceive(txStream_, txChunk_, TX_CHUNK_SIZE, 0);

            if (count == 0) {
                break;  // No more data
            }

            // Start interrupt-based transmission
            if (HAL_UART_Transmit_IT(huart_, txChunk_, count) == HAL_OK) {
                // Wait for TX complete notification from ISR
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));  // 100ms timeout
            }
        }
    }
}

// ============================================================
// ISR CALLBACKS
// ============================================================

void UartEndpoint::DispatchRxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
    auto it = instanceMap.find(huart);
    if (it == instanceMap.end() || !it->second) return;

    UartEndpoint* instance = it->second;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Push received data to RX stream
    xStreamBufferSendFromISR(instance->rxStream_, instance->rxBuffer_, Size, &xHigherPriorityTaskWoken);

    // Restart reception
    HAL_UARTEx_ReceiveToIdle_IT(instance->huart_, instance->rxBuffer_, RX_BUFFER_SIZE);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void UartEndpoint::DispatchTxComplete(UART_HandleTypeDef* huart) {
    auto it = instanceMap.find(huart);
    if (it == instanceMap.end() || !it->second) return;

    UartEndpoint* instance = it->second;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Notify TX task - cast to TaskHandle_t
    vTaskNotifyGiveFromISR((TaskHandle_t)instance->txTaskHandle_, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
// ============================================================
// ENCRYPTION HELPERS
// ============================================================

void UartEndpoint::EncryptInPlace(unsigned char* data, int length) {
    if (!data || length <= 0) return;
    for (int i = 0; i < length; i++)
        data[i] = (unsigned char)(data[i] + 1);
}

void UartEndpoint::DecryptInPlace(unsigned char* data, int length) {
    if (!data || length <= 0) return;
    for (int i = 0; i < length; i++)
        data[i] = (unsigned char)(data[i] - 1);
}

// ============================================================
// HAL CALLBACKS
// ============================================================

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
    UartEndpoint::DispatchRxEventCallback(huart, Size);
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    UartEndpoint::DispatchTxComplete(huart);
}

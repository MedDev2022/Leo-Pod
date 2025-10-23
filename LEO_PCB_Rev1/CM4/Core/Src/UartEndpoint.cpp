#include "UartEndpoint.hpp"
#include <map>

std::map<UART_HandleTypeDef*, UartEndpoint*> UartEndpoint::instanceMap;

UartEndpoint::UartEndpoint(UART_HandleTypeDef* huart) : huart_(huart) {
    instanceMap[huart] = this;
}

bool UartEndpoint::StartReceive(uint8_t* buffer, size_t len) {
    rxBuffer_ = buffer;
    rxLength_ = len;
    return HAL_UART_Receive_IT(huart_, rxBuffer_, rxLength_) == HAL_OK;
}

uint16_t UartEndpoint::SendCommand(const uint8_t* command, size_t length)
{
	if (HAL_UART_Transmit(huart_, command, length,500) == HAL_OK)
		return length;
	else
		return 0;
}

void UartEndpoint::DispatchRxComplete(UART_HandleTypeDef* huart) {
    auto it = instanceMap.find(huart);
    if (it != instanceMap.end()) {
        UartEndpoint* instance = it->second;
        if (instance && instance->rxBuffer_) {
            uint8_t byte = instance->rxBuffer_[0];

            // ✅ Push byte into the queue immediately
            instance->rxQueue_.push_back(byte);

            // Re-arm UART receive
            HAL_UART_Receive_IT(instance->huart_, instance->rxBuffer_, instance->rxLength_);

            // Let derived class process the queue
            instance->onReceiveByte(byte);
        }
    }
}

void UartEndpoint::onReceiveByte(uint8_t byte) {
    // no longer queues here
}

void UartEndpoint::processIncoming() {
    // Base class: do nothing
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    UartEndpoint::DispatchRxComplete(huart);
}

/*****************************************************************************
**	 Name: 	STM32 UART libary												**
**	 MK Medical Device Solutions Ltd.										**
**	 www.mkmeddev.com														**
*****************************************************************************/

#include "uart.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "Client.h"


	void UART::init(USART_TypeDef *  portName ,uint32_t dwBaudRate, bool blParity) {

		if (portName == USART1)
			huart = & huart1;
		else if (portName == USART2)
			huart = & huart2;
		else if (portName == USART3)
			huart = & huart3;
		else if (portName == UART4)
			huart = & huart4;


		huart->Instance = portName;
		huart->Init.BaudRate = dwBaudRate;
		huart->Init.WordLength = blParity ? UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
		huart->Init.Parity = blParity ? UART_PARITY_ODD : UART_PARITY_NONE;
		huart->Init.StopBits = UART_STOPBITS_1;
		huart->Init.Mode = UART_MODE_TX_RX;
		huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
		huart->Init.OverSampling = UART_OVERSAMPLING_16;
		huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
		huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
		if (HAL_UART_Init(huart) != HAL_OK)
		{
			//error handler
		}

	}

 	bool UART::received(void) {

		return RX_IRQ_STATUS;
	}

	//void UART::StartReceive(uint8_t *rxBuffer, size_t length){
 	void UART::StartReceive(void *rxBuffer, size_t length){




 		uint8_t buff1[140]={0};
 		uint8_t* buff;

 		HAL_UART_Receive_IT(huart, (uint8_t*)rxBuffer, 132);

 		return;


 		if(HAL_UART_Receive(huart, (uint8_t*)rxBuffer, 132,50)!=HAL_TIMEOUT){
 			__NOP();
 			client.println("Sync received");
 		}
 		else
 		{
 			client.println("Timeout");
 		}

 		return;

 		//HAL_UART_Receive_IT(huart, (uint8_t*)rxBuffer, length);
 		if(HAL_UART_Receive(huart, (uint8_t*)rxBuffer, length,10000)!=HAL_TIMEOUT)
 		{
 			__NOP();
 			buff = (uint8_t*)rxBuffer;

 			if(buff[0] == 0xAA){

 				if(HAL_UART_Receive(huart, (uint8_t*)rxBuffer, 1,10000)!=HAL_TIMEOUT)

 				{
 		 			buff = (uint8_t*)rxBuffer;

 		 			if(buff[0] == 0x55){

 				HAL_UART_Receive(huart, buff1, 130,10000)!=HAL_TIMEOUT;
 				//this->send(rxBuffer, length);
 				client.println("Sync received");
 				}
 				}
 			}


 		}
 		else
 		{
 			__NOP();

 		}


	}

 	HAL_StatusTypeDef UART::receive(void* pData, uint32_t ulDataSize) {

		RX_IRQ_STATUS = false;
		HAL_UART_Abort(huart);
		HAL_UART_Receive_IT(huart, (uint8_t *)pData, ulDataSize);

	}

	HAL_StatusTypeDef UART::receive() {
	}

	void UART::KillRx() {
		RX_IRQ_STATUS = false;
		HAL_UART_Abort(huart);
	}

	HAL_StatusTypeDef UART::send(void* pData, uint32_t ulDataSize) {
		HAL_UART_Transmit(huart, (uint8_t*) pData, ulDataSize, HAL_MAX_DELAY);}

	void UART::println(const char* message) {

		const char * newLine = "\r\n";
	    // Calculate the total length of the message including the new line
	    size_t messageLength = strlen(message);
	    size_t newLineLength = strlen(newLine);
	    size_t totalLength = messageLength + newLineLength;

	    // Dynamically allocate a buffer to hold the concatenated message
	    char* combinedMessage = new char[totalLength + 1];  // +1 for null-terminator

	    // Concatenate message and new line
	    strcpy(combinedMessage, message);
	    strcat(combinedMessage, newLine);

	    // Send the concatenated message
	    HAL_UART_Transmit(huart, reinterpret_cast<uint8_t*>(combinedMessage), totalLength, HAL_MAX_DELAY);

	    // Free the allocated memory
	    delete[] combinedMessage;
	}

	bool UART::IsSent(void){
		return true;}

	USART_TypeDef * UART::getHuart(){
		return portName;
	}





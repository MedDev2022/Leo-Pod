

/*****************************************************************************
**																			**
**	 Name: 	Blackfin 561 UART libary										**
**																			**
*****************************************************************************/

#ifndef SRC_UART_H_
#define SRC_UART_H_

#include "main.h"





	class UART {
		public:

	    	void open(USART_TypeDef *  portName) {init(portName);}

			void init(USART_TypeDef *  port = USART1, uint32_t dwBaudRate = 57600, bool blParity = false);

			bool received(void);

			void StartReceive(void *rxBuffer, size_t length);

			HAL_StatusTypeDef receive(void* pData, uint32_t ulDataSize);
			HAL_StatusTypeDef receive();

			HAL_StatusTypeDef send(void* pData, uint32_t ulDataSize);

			void KillRx(void);

			void ReviveRx(void);

			bool IsSent(void);

		    void println(const char* message);

			bool RX_IRQ_STATUS = false;

			USART_TypeDef * getHuart();
			UART_HandleTypeDef * huart;

		private:
			USART_TypeDef * portName;

	};

static UART Serial1;

#endif /* SRC_UART_H_ */

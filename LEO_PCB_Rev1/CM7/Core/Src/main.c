/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "resmgr_utility.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
//#define SHARED_FOCUS_IPC_OWNER
#include "shared_focus_ipc.h"
#include <string.h>
//__attribute__((section(".shared_ram")))
//volatile SharedMailbox_t g_mb = {0};

#define g_mb (*(volatile SharedMailbox_t*)0x30010000)

/* ============================================================================
 * FPGA RECEPTION BUFFERS - Replace your existing buffers
 * ============================================================================ */
#define FPGA_PACKET_SIZE    516     /* 512 data + 2 header bytes + 2 trailer */

static uint8_t RxBuff1[1024];       /* DMA receive buffer */
static uint8_t RxBuff[FPGA_PACKET_SIZE*2];  /* Validated packet */
static volatile uint8_t RXBuffReady = 0;
static volatile uint16_t RX_size = 0;

/* Extern declaration */
//extern volatile FocusCmd_t focusCmd;

//extern volatile SharedMailbox_t g_mb;

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */



/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */


uint8_t txBuf[] = "Hello DMA UART6\r\n";



/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart6_rx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART6_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// uint8_t RxBuff1[70000];
//uint8_t RxBuff2[200];
// uint8_t RxBuff3[20];
// uint8_t RxBuff[70000];
//
//
// uint8_t RXBuffReady = 0 ;
// uint8_t RX_size = 0;


/* ============================================================================
 * Debug: Track mailbox state
 * ============================================================================ */
static uint32_t dbg_requests_seen = 0;
static uint32_t dbg_responses_sent = 0;
static uint32_t dbg_last_req_seq = 0;
static uint32_t dbg_fpga_packets = 0;

/* ============================================================================
 * CheckMailboxRequest - Check for M4 requests and respond
 * This is SEPARATE from FPGA data processing so we can test communication
 * ============================================================================ */
void CheckMailboxRequest(void)
{

	/* Invalidate cache to see M4's writes */
    SCB_InvalidateDCache_by_Addr((uint32_t*)&g_mb, sizeof(g_mb));

    /* Check if there's a new request we haven't processed */
    if (g_mb.req_cmd == 0 || g_mb.req_seq == 0) {
        return;  /* No request pending */
    }

    /* Check if we already responded to this sequence */
    if (g_mb.req_seq == dbg_last_req_seq) {
        return;  /* Already processed this request */
    }

    /* New request! */
    dbg_requests_seen++;
    dbg_last_req_seq = g_mb.req_seq;

    /* Store request values locally before any cache operations */
    uint32_t req_seq = g_mb.req_seq;
    uint32_t req_step = g_mb.req_step;
    uint8_t req_cmd = g_mb.req_cmd;
    RXBuffReady = 0;

    while (RXBuffReady == 0)
	{
		HAL_Delay(1);
	}

    /* Process based on command */
    if (req_cmd == MB_REQ_CAPTURE) {
        /* For capture request, check if we have FPGA data */
        if (RXBuffReady) {
            /* Process FPGA data */
            uint64_t sumX = 0;
            uint64_t sumY = 0;

            for (int i = 0; i < XY_SAMPLES; i++) {
                uint32_t offset = i * 8;

                uint32_t xVal = (uint32_t)(
                    ((uint32_t)RxBuff[offset + 0])       |
                    ((uint32_t)RxBuff[offset + 1] << 8)  |
                    ((uint32_t)RxBuff[offset + 2] << 16) |
                    ((uint32_t)RxBuff[offset + 3] << 24)
                );

                uint32_t yVal = (uint32_t)(
                    ((uint32_t)RxBuff[offset + 4])       |
                    ((uint32_t)RxBuff[offset + 5] << 8)  |
                    ((uint32_t)RxBuff[offset + 6] << 16) |
                    ((uint32_t)RxBuff[offset + 7] << 24)
                );

               g_mb.x[i] = xVal;
               //g_mb.y[i] = yVal;
                g_mb.y[i] =0;
                sumX += xVal;
                //sumY += yVal;
                sumY += 0;
            }
  //          HAL_UART_Transmit(&huart6, RxBuff, (FPGA_PACKET_SIZE-1), 1000);

            HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
            g_mb.sumX = sumX;
            g_mb.sumY = sumY;
            g_mb.rsp_status = 0;  /* OK - have data */
         //   RXBuffReady = 0;
            dbg_fpga_packets++;
        } else {
            g_mb.sumX = 0;
            g_mb.sumY = 0;
            g_mb.rsp_status = 1;  /* No FPGA data available */
        }
    } else {
        g_mb.rsp_status = 2;  /* Unknown command */
    }

    /* Set response fields */
    g_mb.rsp_seq = req_seq;
    g_mb.rsp_step = req_step;

    /* Memory barrier BEFORE setting ready flag */
    __DMB();

    /* NOW set ready flag */
    g_mb.rsp_ready = 1;

    /* Clean ENTIRE mailbox so M4 sees ALL updates */
    SCB_CleanDCache_by_Addr((uint32_t*)&g_mb, sizeof(g_mb));

    dbg_responses_sent++;
}

/* Legacy function name for compatibility */
void ProcessFPGAData(void)
{
    CheckMailboxRequest();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
  int32_t timeout;
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
			// while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
			//  if ( timeout < 0 )
			//  {
			//  Error_Handler();
			//  }
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* Clear mailbox BEFORE releasing M4 */
//  memset((void*)&g_mb, 0, sizeof(g_mb));
//  __DMB();  // Ensure writes complete
//  SCB_CleanDCache_by_Addr((uint32_t*)&g_mb, sizeof(g_mb));

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();

//SCB_CleanDCache_by_Addr((uint32_t*)&focusCmd, sizeof(focusCmd));

/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));


if ( timeout < 0 )
{
Error_Handler();
}
/* USER CODE END Boot_Mode_Sequence_2 */
  /* Resource Manager Utility initialisation ---------------------------------*/
  MX_RESMGR_UTILITY_Init();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */



  HAL_UARTEx_ReceiveToIdle_DMA(&huart6, RxBuff1, 1000);
  __HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT); // Disable Half Transfer interrupt
  __HAL_UART_ENABLE_IT(&huart6, UART_IT_IDLE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      /* Process FPGA data if available */
      ProcessFPGAData();

      /* Heartbeat LED */


      HAL_Delay(50);


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_CSI;
  RCC_OscInitStruct.CSIState = RCC_CSI_ON;
  RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_CSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 240;
  RCC_OscInitStruct.PLL.PLLP = 6;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_0;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  if (ResMgr_Request(RESMGR_ID_USART6, RESMGR_FLAGS_ACCESS_NORMAL | \
                  RESMGR_FLAGS_CPU1 , 0, NULL) != RESMGR_OK)
  {
    /* USER CODE BEGIN RESMGR_UTILITY_USART6 */
    Error_Handler();
    /* USER CODE END RESMGR_UTILITY_USART6 */
  }
  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 1000000;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart6.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart6, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart6, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  if (ResMgr_Request(RESMGR_ID_DMA1, RESMGR_FLAGS_ACCESS_NORMAL | \
                  RESMGR_FLAGS_CPU1 , 0, NULL) != RESMGR_OK)
  {
    /* USER CODE BEGIN RESMGR_UTILITY_DMA1 */
    Error_Handler();
    /* USER CODE END RESMGR_UTILITY_DMA1 */
  }
  if (ResMgr_Request(RESMGR_ID_DMA2, RESMGR_FLAGS_ACCESS_NORMAL | \
                  RESMGR_FLAGS_CPU1 , 0, NULL) != RESMGR_OK)
  {
    /* USER CODE BEGIN RESMGR_UTILITY_DMA2 */
    Error_Handler();
    /* USER CODE END RESMGR_UTILITY_DMA2 */
  }
  if (ResMgr_Request(RESMGR_ID_DMAMUX1, RESMGR_FLAGS_ACCESS_NORMAL | \
                  RESMGR_FLAGS_CPU1 , 0, NULL) != RESMGR_OK)
  {
    /* USER CODE BEGIN RESMGR_UTILITY_DMAMUX1 */
    Error_Handler();
    /* USER CODE END RESMGR_UTILITY_DMAMUX1 */
  }

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : MCU_LED_2_Pin */
  GPIO_InitStruct.Pin = MCU_LED_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MCU_LED_1_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{


    if (huart->Instance == USART6)
    {

    RX_size = size;
    	// Clear relevant UART flags before processing
    	__HAL_UART_DISABLE_IT(&huart6, UART_IT_IDLE);  // Ensure IDLE detection is enabled

        if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_RXNE)) {
            __HAL_UART_CLEAR_FLAG(&huart6, UART_FLAG_RXNE); // Clear Receive Not Empty flag
            // Process received data
        }

        if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_IDLE)) {
            __HAL_UART_CLEAR_FLAG(&huart6, UART_FLAG_IDLE); // Clear idle flag
            // Process received data
        }

        if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_ORE)) {
            __HAL_UART_CLEAR_FLAG(&huart6, UART_FLAG_ORE); // Clear overrun flag
            // Handle overrun condition
        }

        if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_FE)) {
            __HAL_UART_CLEAR_FLAG(&huart6, UART_FLAG_FE); // Clear framing error flag
            // Handle framing error
        }

        if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_NE)) {
            __HAL_UART_CLEAR_FLAG(&huart6, UART_FLAG_NE); // Clear noise error flag
            // Handle noise error
        }

        if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_PE)) {
            __HAL_UART_CLEAR_FLAG(&huart6, UART_FLAG_PE); // Clear Parity Error flag
            // Handle noise error
        }


        if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_TC)) {
            __HAL_UART_CLEAR_FLAG(&huart6, UART_FLAG_TC); // Clear Transmission Complete flag
            // Process received data
        }

        /* Validate: correct size AND trailer bytes 0xAA 0x55 */
        if (size == FPGA_PACKET_SIZE &&
            RxBuff1[512] == 0xAA &&
            RxBuff1[513] == 0x55 &&
            RxBuff1[514] == 0xAA &&
			RxBuff1[515] == 0x55)

        {

            /* Buffer FPGA data if we don't have pending data */
            /* Don't check mailbox here - just buffer the latest valid packet */
            if (RXBuffReady == 0) {
            	HAL_GPIO_TogglePin(MCU_LED_1_GPIO_Port, MCU_LED_1_Pin);
                memcpy(RxBuff, RxBuff1, FPGA_PACKET_SIZE);
                RXBuffReady = 1;



            }


        }



        /* Always restart DMA reception */
        HAL_UARTEx_ReceiveToIdle_DMA(huart, RxBuff1, 1000);
        __HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT);
        __HAL_UART_ENABLE_IT(&huart6, UART_IT_IDLE);

    }


}





/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#include "resmgr_utility.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "cmsis_os2.h"
#include "FreeRTOS.h"

extern SPI_HandleTypeDef hspi2;
extern I2C_HandleTypeDef hi2c2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart7;
extern UART_HandleTypeDef huart8;

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */



/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TMP_I2C_SDA_Pin GPIO_PIN_0
#define TMP_I2C_SDA_GPIO_Port GPIOF
#define TMP_I2C_SCL_Pin GPIO_PIN_1
#define TMP_I2C_SCL_GPIO_Port GPIOF
#define MCU_LED_1_Pin GPIO_PIN_0
#define MCU_LED_1_GPIO_Port GPIOC
#define MCU_LED_2_Pin GPIO_PIN_1
#define MCU_LED_2_GPIO_Port GPIOC
#define MCU_2_LSR_TXD_Pin GPIO_PIN_2
#define MCU_2_LSR_TXD_GPIO_Port GPIOA
#define MCU_2_LSR_RXD_Pin GPIO_PIN_3
#define MCU_2_LSR_RXD_GPIO_Port GPIOA
#define LRF_LASER_ON_N_Pin GPIO_PIN_0
#define LRF_LASER_ON_N_GPIO_Port GPIOB
#define SDI_RESET_Pin GPIO_PIN_1
#define SDI_RESET_GPIO_Port GPIOB
#define RP_TEC_ON_Pin GPIO_PIN_14
#define RP_TEC_ON_GPIO_Port GPIOF
#define RP_COOL_HEAT_Pin GPIO_PIN_15
#define RP_COOL_HEAT_GPIO_Port GPIOF
#define FPGA_RD_DAT0_Pin GPIO_PIN_0
#define FPGA_RD_DAT0_GPIO_Port GPIOG
#define FPGA_RD_DAT1_Pin GPIO_PIN_1
#define FPGA_RD_DAT1_GPIO_Port GPIOG
#define TMP_I2C_RST_N_Pin GPIO_PIN_13
#define TMP_I2C_RST_N_GPIO_Port GPIOE
#define TMP_I2C_ADR0_Pin GPIO_PIN_14
#define TMP_I2C_ADR0_GPIO_Port GPIOE
#define TMP_I2C_ADR1_Pin GPIO_PIN_15
#define TMP_I2C_ADR1_GPIO_Port GPIOE
#define MCU_2_DAYCAM_TXD_Pin GPIO_PIN_10
#define MCU_2_DAYCAM_TXD_GPIO_Port GPIOB
#define MCU_2_DAYCAM_RXD_Pin GPIO_PIN_11
#define MCU_2_DAYCAM_RXD_GPIO_Port GPIOB
#define DAYCAM_TEC_ON_Pin GPIO_PIN_10
#define DAYCAM_TEC_ON_GPIO_Port GPIOD
#define DAYCAM_COOL_HEAT_Pin GPIO_PIN_11
#define DAYCAM_COOL_HEAT_GPIO_Port GPIOD
#define IR_TEC_ON_Pin GPIO_PIN_12
#define IR_TEC_ON_GPIO_Port GPIOD
#define IR_COOL_HEAT_Pin GPIO_PIN_13
#define IR_COOL_HEAT_GPIO_Port GPIOD
#define FPGA_RD_DAT2_Pin GPIO_PIN_2
#define FPGA_RD_DAT2_GPIO_Port GPIOG
#define FPGA_RD_DAT3_Pin GPIO_PIN_3
#define FPGA_RD_DAT3_GPIO_Port GPIOG
#define FPGA_RD_DAT4_Pin GPIO_PIN_4
#define FPGA_RD_DAT4_GPIO_Port GPIOG
#define FPGA_RD_DAT5_Pin GPIO_PIN_5
#define FPGA_RD_DAT5_GPIO_Port GPIOG
#define FPGA_RD_DAT6_Pin GPIO_PIN_6
#define FPGA_RD_DAT6_GPIO_Port GPIOG
#define FPGA_RD_DAT7_Pin GPIO_PIN_7
#define FPGA_RD_DAT7_GPIO_Port GPIOG
#define FPGA_FIFO_READY_Pin GPIO_PIN_8
#define FPGA_FIFO_READY_GPIO_Port GPIOG
#define MCU_2_HOST_TXD_Pin GPIO_PIN_9
#define MCU_2_HOST_TXD_GPIO_Port GPIOA
#define MCU_2_HOST_RXD_Pin GPIO_PIN_10
#define MCU_2_HOST_RXD_GPIO_Port GPIOA
#define SERFLSH_RST_N_Pin GPIO_PIN_12
#define SERFLSH_RST_N_GPIO_Port GPIOC
#define COOLING_ON_Pin GPIO_PIN_0
#define COOLING_ON_GPIO_Port GPIOD
#define HEATER_SDI_CONV_Pin GPIO_PIN_1
#define HEATER_SDI_CONV_GPIO_Port GPIOD
#define HEATER_ON_IRCAM_Pin GPIO_PIN_2
#define HEATER_ON_IRCAM_GPIO_Port GPIOD
#define HEATER_MISC_Pin GPIO_PIN_3
#define HEATER_MISC_GPIO_Port GPIOD
#define FPGA_FIFO_RDREQ_Pin GPIO_PIN_9
#define FPGA_FIFO_RDREQ_GPIO_Port GPIOG
#define G_THRSHLD_Pin GPIO_PIN_7
#define G_THRSHLD_GPIO_Port GPIOB
#define MCU_2_CLI_RXD_Pin GPIO_PIN_8
#define MCU_2_CLI_RXD_GPIO_Port GPIOB
#define MCU_2_CLI_TXD_Pin GPIO_PIN_9
#define MCU_2_CLI_TXD_GPIO_Port GPIOB
#define MCU_2_IR_RXD_Pin GPIO_PIN_0
#define MCU_2_IR_RXD_GPIO_Port GPIOE
#define MCU_2_IR_TXD_Pin GPIO_PIN_1
#define MCU_2_IR_TXD_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

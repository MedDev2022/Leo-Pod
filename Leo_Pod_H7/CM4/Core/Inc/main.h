/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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


static UART_HandleTypeDef huart1;
static UART_HandleTypeDef huart2;
static UART_HandleTypeDef huart3;
static UART_HandleTypeDef huart4;




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
#define BTN_FOCUS_NEAR_Pin GPIO_PIN_6
#define BTN_FOCUS_NEAR_GPIO_Port GPIOC
#define BTN_FOCUS_FAR_Pin GPIO_PIN_7
#define BTN_FOCUS_FAR_GPIO_Port GPIOC
#define BTN_ZOOM_OUT_Pin GPIO_PIN_8
#define BTN_ZOOM_OUT_GPIO_Port GPIOC
#define BTN_ZOOM_IN_Pin GPIO_PIN_9
#define BTN_ZOOM_IN_GPIO_Port GPIOC
#define USER_LED2_Pin GPIO_PIN_1
#define USER_LED2_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

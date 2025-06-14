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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
#define BTN1_Pin GPIO_PIN_2
#define BTN1_GPIO_Port GPIOI
#define BTN1_EXTI_IRQn EXTI2_IRQn
#define BTN2_Pin GPIO_PIN_6
#define BTN2_GPIO_Port GPIOE
#define BTN2_EXTI_IRQn EXTI9_5_IRQn
#define BTN3_Pin GPIO_PIN_8
#define BTN3_GPIO_Port GPIOA
#define BTN3_EXTI_IRQn EXTI9_5_IRQn
#define BTN4_Pin GPIO_PIN_1
#define BTN4_GPIO_Port GPIOK
#define BTN4_EXTI_IRQn EXTI1_IRQn
#define ECHO_Pin GPIO_PIN_8
#define ECHO_GPIO_Port GPIOF
#define TRIGF_Pin GPIO_PIN_0
#define TRIGF_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

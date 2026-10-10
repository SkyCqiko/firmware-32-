/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32f4xx_hal.h"

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* Private defines -----------------------------------------------------------*/
#define PWMB_Pin GPIO_PIN_0
#define PWMB_GPIO_Port GPIOB
#define PWMA_Pin GPIO_PIN_1
#define PWMA_GPIO_Port GPIOB

#define Servo_PWM_Pin GPIO_PIN_12
#define Servo_PWM_GPIO_Port GPIOD

#define TFT_BLK_Pin GPIO_PIN_11
#define TFT_BLK_GPIO_Port GPIOA
#define TFT_RST_Pin GPIO_PIN_12
#define TFT_RST_GPIO_Port GPIOA
#define TFT_CS_Pin GPIO_PIN_15
#define TFT_CS_GPIO_Port GPIOA
#define TFT_SCL_Pin GPIO_PIN_10
#define TFT_SCL_GPIO_Port GPIOC
#define TFT_DC_Pin GPIO_PIN_11
#define TFT_DC_GPIO_Port GPIOC
#define TFT_SDA_Pin GPIO_PIN_12
#define TFT_SDA_GPIO_Port GPIOC

#define OV_SCL_Pin GPIO_PIN_3
#define OV_SCL_GPIO_Port GPIOB
#define OV_SDA_Pin GPIO_PIN_5
#define OV_SDA_GPIO_Port GPIOB
/* OV2640 电源控制引脚: 以 project_Car1.ioc 为准 (PB4 = OV_PWDN, PB8 = OV_RST) */
#define OV_PWDN_Pin GPIO_PIN_4
#define OV_PWDN_GPIO_Port GPIOB
#define OV_RST_Pin GPIO_PIN_8
#define OV_RST_GPIO_Port GPIOB
/* 兼容保留: 旧代码把 PWDN 写在 PB9 上, 原理图确认前一起拉低; 确认 PWDN 接的是 PB4 后可删除 */
#define OV_PWDN_ALT_Pin GPIO_PIN_9
#define OV_PWDN_ALT_GPIO_Port GPIOB

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

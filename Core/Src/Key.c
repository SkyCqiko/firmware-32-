#include "main.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_gpio.h"
#include <stdint.h>


/*======*  在这里修改按键配置  *======*/
#define Key_GPIO  GPIOC
#define Key_1     GPIO_PIN_0
#define Key_2     GPIO_PIN_1
#define Key_3     GPIO_PIN_2
/*===================================*/
uint8_t KeyNum;

/**
*@brief 按键初始化函数
*@return None
**/
void Key_Init(void)
{
  /*配置修改后此处按需修改*/
  __HAL_RCC_GPIOC_CLK_ENABLE();
  /*=======================*/

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = Key_1 | Key_2 | Key_3;
  GPIO_InitStruct.Mode =GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Key_GPIO, &GPIO_InitStruct);
}


/**
*@brief 按键读取函数
*@return 按键值
**/
uint8_t Key_readBit(void)
{
    if(HAL_GPIO_ReadPin(Key_GPIO,Key_1) == GPIO_PIN_RESET)KeyNum = 1;
    if(HAL_GPIO_ReadPin(Key_GPIO,Key_2) == GPIO_PIN_RESET)KeyNum = 2;
    if(HAL_GPIO_ReadPin(Key_GPIO,Key_3) == GPIO_PIN_RESET)KeyNum = 3;
    return KeyNum;
}
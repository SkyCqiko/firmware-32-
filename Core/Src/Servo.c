#include "main.h"
#include "stm32f4xx_hal_tim.h"

TIM_HandleTypeDef htim4;

/**
*@brief 舵机初始化函数(TIM4_channel1)
*@param: None
*@return None
**/
void Servo_Init(void)
{
 TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 83;     /* 84MHz/(83+1)=1MHz, 周期19999 -> 50Hz 舵机 */
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 19999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim4);
  HAL_TIM_Base_Start(&htim4);
  
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig);
  HAL_TIM_PWM_Init(&htim4);
  HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_1);

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1);
  HAL_TIM_MspPostInit(&htim4);
  HAL_TIM_OC_Start(&htim4,TIM_CHANNEL_1 );
}

/**
*@brief 舵机偏转角度函数
*@param Angle 设定角度
*@return None
**/
void Servo_ChangeAngle(uint8_t Angle)
{
    uint16_t pulse;
    if (Angle > 180) Angle = 180;
    // 0.5 ms ~ 2.5 ms，对应 0° ~ 180°
    pulse = 500 + ((uint32_t)Angle * 2000) / 180;
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pulse);
}

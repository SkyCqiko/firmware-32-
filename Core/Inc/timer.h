#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/*==============================================================================
 *  普通定时器(基本定时)中断 —— 10ms 周期
 *  ---------------------------------------------------------------------------
 *  使用外设 : TIM2 (APB1 总线, 仅作基本定时用, 不占用任何 IO 引脚)
 *  定时周期 : 10ms (TIM_CLK 84MHz / (83999+1) = 1kHz, 计数 10 次 => 100Hz)
 *  ---------------------------------------------------------------------------
 *  模块分工 :
 *    timer.c  : 本文件负责"初始化"与"启动/停止", 不含中断服务函数
 *    main.c   : 中断服务函数 TIM2_IRQHandler() 放在 main.c 中(按项目要求)
 *
 *  其它已在用外设, 不要重复使用 :
 *    TIM1             -> PE9/PE10 编码器1    (Drving.c)
 *    TIM3_MspPostInit -> PB0/PB1 电机 PWM     (main.c)
 *    TIM4             -> PD12    舵机 PWM     (Servo.c)
 *    TIM5             -> PA0/PA1 编码器2     (Drving.c)
 *  ---------------------------------------------------------------------------
 *  中断优先级 : 抢占 6, 子优先级 0
 *    SysTick = 15(HAL 默认), DCMI/DMA2 = 5
 *    取 6 使定时器中断优先级高于 SysTick、低于摄像头中断, 可按需调整。
 *
 *  【注意】本工程 SystemClock_Config() 把 APB1 分频设为 4(HCLK 168MHz),
 *  因此 TIM2 时钟 = PCLK1 x 2 = 84MHz(timer.c 里按此值算预分频)。
 *  若以后改动时钟树, 只需修改 timer.c 中 TIMER_APB1_TIM_CLK_MHZ。
 *  想改中断周期(如 1ms/5ms), 只改 timer.c 中的 TIMER_PERIOD_MS 一个宏。
 *============================================================================*/

/* 定时中断计数值(每进一次中断 +1), 由 main.c 的中断函数累加; 每 +1 代表 10ms */
extern volatile uint32_t g_tim2_tick;

/**
  *@brief 普通定时器(TIM2)初始化: 10ms 定时中断, 初始化完成后自动开启中断
  *@param: None
  *@return None
  **/
void Timer_Init(void);

/**
  *@brief 启动定时器中断(重复调用无副作用, 只影响计数器使能)
  *@param: None
  *@return None
  **/
void Timer_Start(void);

/**
  *@brief 停止定时器中断
  *@param: None
  *@return None
  **/
void Timer_Stop(void);

#endif

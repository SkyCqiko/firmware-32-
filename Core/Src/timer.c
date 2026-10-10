#include "main.h"
#include "timer.h"
#include "stm32f4xx_hal_tim.h"

/*==============================================================================
 *  普通定时器(基本定时)中断初始化 —— 10ms
 *  ---------------------------------------------------------------------------
 *  TIM2 挂在 APB1 上, 本工程 APB1 分频 = 4, 故 TIM2 时钟 = PCLK1 x 2 = 84MHz
 *  (与 Servo.c 里 TIM4 的 84MHz 一致)
 *
 *      预分频 : 84MHz / (83999+1) = 1kHz   ->  计数器每 1ms 加 1
 *      周期   : 1kHz 计数 10 次    = 10ms   ->  每 10ms 产生一次更新中断 (100Hz)
 *
 *  两个宏的含义(想改中断周期时看这里):
 *      TIMER_COUNTER_CLK_HZ : 计数器的工作频率, 也是定时精度(1000Hz = 1ms)
 *      TIMER_PERIOD_MS      : 每多少次计数产生一次中断, 即中断周期
 *    中断周期 = TIMER_PERIOD_MS / TIMER_COUNTER_CLK_HZ
 *    例: 1ms 中断 -> COUNTER_CLK 1000, PERIOD_MS 1
 *        5ms 中断 -> COUNTER_CLK 1000, PERIOD_MS 5
 *============================================================================*/
#define TIMER_APB1_TIM_CLK_MHZ   84U     /* TIM2 输入时钟(MHz), 时钟树改动时只改这里 */
#define TIMER_COUNTER_CLK_HZ     1000U   /* 计数器时钟 1kHz, 即 1 个计数 = 1ms */
#define TIMER_PERIOD_MS          10U     /* 计数 10 次产生一次中断 => 10ms */

/* TIM2 实际输入时钟(Hz): 84MHz */
#define TIMER_INPUT_CLK_HZ   (TIMER_APB1_TIM_CLK_MHZ * 1000000U)

/* 预分频与周期寄存器(PSC/ARR)的实际取值 */
#define TIMER_PSC_VALUE      ((TIMER_INPUT_CLK_HZ / TIMER_COUNTER_CLK_HZ) - 1U)
#define TIMER_ARR_VALUE      (TIMER_PERIOD_MS - 1U)

/* 编译期自检: 确认算出来的 PSC/ARR 就是期望值
   (改中断周期时, 下面的期望值也要一起改; 写错会直接编译失败, 不会烧进板子才发现)
   说明: 这里只用 == 精确比较, 不用范围比较 —— 本工具链的预处理器/常量折叠对
         >65535 的大数比较不可靠, 实测 #if 83999 > 65535 会错误地判为真。 */
_Static_assert(TIMER_PSC_VALUE == 83999U, "PSC 应为 83999(84MHz/1kHz-1), 请检查 TIMER_COUNTER_CLK_HZ");
_Static_assert(TIMER_ARR_VALUE == 9U,     "ARR 应为 9(10ms/1ms-1), 请检查 TIMER_PERIOD_MS");
_Static_assert(TIMER_PSC_VALUE >= 1U,     "预分频至少为 1, 否则计数时钟等于输入时钟");

/* TIM2 句柄: 中断函数在 main.c 中通过 extern 使用 */
TIM_HandleTypeDef htim2;

/* 10ms 计数值, 在 main.c 的 TIM2 中断函数中累加(每 +1 代表过去 10ms) */
volatile uint32_t g_tim2_tick = 0;

/**
  *@brief 普通定时器(TIM2)初始化: 10ms 定时中断, 初始化完成后自动开启中断
  *@param: None
  *@return None
  **/
void Timer_Init(void)
{
  /* 1. 基本定时参数: 预分频 + 周期(自动重装载值) */
  htim2.Instance = TIM2;
  /* 84MHz / 1kHz = 84000 个时钟 -> 1 个计数, 故预分频 = 84000-1 = 83999 */
  htim2.Init.Prescaler = TIMER_PSC_VALUE;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  /* 1kHz 计数 10 次 => 10ms */
  htim2.Init.Period = TIMER_ARR_VALUE;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  /* 2. 时基初始化(内部会调用 HAL_TIM_Base_MspInit 打开 TIM2 时钟) */
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  /* 3. 关闭 TIM2 的刹车/死区等主从输出配置, 保持纯定时用途 */
  {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
    {
      Error_Handler();
    }
  }

  /* 4. 配置并使能 NVIC(中断优先级组为 NVIC_PRIORITYGROUP_4, 只用抢占优先级) */
  HAL_NVIC_SetPriority(TIM2_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);

  /* 5. 开启更新中断并启动计数, 之后每 10ms 进入一次 TIM2_IRQHandler(main.c) */
  Timer_Start();
}

/**
  *@brief 启动定时器中断
  *@param: None
  *@return None
  **/
void Timer_Start(void)
{
  g_tim2_tick = 0;
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  *@brief 停止定时器中断
  *@param: None
  *@return None
  **/
void Timer_Stop(void)
{
  HAL_TIM_Base_Stop_IT(&htim2);
}

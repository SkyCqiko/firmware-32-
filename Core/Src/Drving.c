#include "main.h"
#include "Drving.h"
#include "stm32f4xx_hal_tim.h"
#include <stdint.h>

/**
*==========================================================
*@file:编码电机驱动文件
*@brief:TIM1和TIM5为编码器定时器，TIM3为电机PWM定时器
*@attention：
*  编码器接口初始化 —— 正交编码器模式
*  ---------------------------------------------------------
*      TI1(A相) --\
*                  >-- 四倍频计数(TI12 模式, 每个边沿都计数)
*      TI2(B相) --/
*
*  采样策略:
*      硬件计数器是 16 位的, 会自由回绕; 本模块用 Encoder_Update()
*      按固定周期读取并累加成 32 位位置, 同时算出该周期的增量。
*      中断里只做"读寄存器 + 累加", 主循环只读取结果, 两者不共享可变状态。
*==========================================================
**/

/*==============================================================================
 *  编码器参数
 *============================================================================*/

/* 计数方向取反开关: 实测方向反了就把对应值由 0 改成 1(另一种方法是直接对调 A/B 接线) */
#define ENC1_INVERT_DIR   0U   /* 编码器1(TIM1) 方向取反 */
#define ENC2_INVERT_DIR   0U   /* 编码器2(TIM5) 方向取反 */

/* 编码器输入滤波(0~15, 数字滤波器的采样点数):
 *
 *      滤波能挡住的脉冲宽度 ≈ N / fSAMPLING
 *      fSAMPLING = fDTS / 32
 *      TIM1 在 APB2 上, fDTS = 168MHz -> fSAMPLING = 5.25MHz
 *      TIM5 在 APB1 上, fDTS =  84MHz -> fSAMPLING = 2.625MHz
 *
 *      取值 10 -> N = 8 个采样点:
 *          TIM1: 8 / 5.25MHz  ≈ 1.5µs  (约 650kHz 以下的脉冲才滤得掉)
 *          TIM5: 8 / 2.625MHz ≈ 3.0µs  (约 330kHz 以下的脉冲才滤得掉)
 *
 *      【不是越大越好】数值越大滤波越强, 但可计数的最高频率也越低;
 *      若噪声频率高于上面的门槛, 加大滤波值没有意义, 必须从接线/屏蔽/
 *      硬件 RC 滤波解决。改动此值后 TIM1/TIM5 的滤波门槛不同, 见上面的换算。 */
#define ENC_INPUT_FILTER  10U

/* 编译期自检: IC1Filter/IC2Filter 是 4 位字段, 只能写 0~15 */
_Static_assert(ENC_INPUT_FILTER <= 15U, "编码器输入滤波值必须在 0~15 之间");

/* 两个编码器的定时器句柄 */
TIM_HandleTypeDef htim1;   /* 编码器1: PE9/PE10 */
TIM_HandleTypeDef htim5;   /* 编码器2: PA0/PA1  */
TIM_HandleTypeDef htim3;   /*电机PWM驱动: PB0/PB1*/

/* 累计位置(32 位有符号), 由 Encoder_Update() 在中断里更新、主循环里读取 */
volatile int32_t g_enc1_count = 0;
volatile int32_t g_enc2_count = 0;

/* 最近一个采样周期的脉冲增量 */
volatile int32_t g_enc1_delta = 0;
volatile int32_t g_enc2_delta = 0;

/* 上一次采样时的硬件计数器值, 只在 Encoder_Update() 内部使用,
   所以是 static 且不需要 volatile(仅被单一上下文访问) */
static uint16_t s_last_raw_enc1 = 0;
static uint16_t s_last_raw_enc2 = 0;

/**
  *@brief 编码器接口初始化: 编码器1(TIM1, PE9/PE10) + 编码器2(TIM5, PA0/PA1)
  *       采用正交编码器四倍频模式, 初始化完成后编码器立即开始计数
  *@param: None
  *@return None
  **/
void Encoder_Init(void)
{
  TIM_Encoder_InitTypeDef sEncoderConfig = {0};

  /* ---------------- 编码器1: TIM1, PE9(A相)/PE10(B相) ---------------- */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;                              /* 编码器模式下分频无意义, 保持 0 */
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;           /* 计数方向由硬件按 A/B 相位决定 */
  htim1.Init.Period = 65535;                             /* 16 位量程 */
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  /* 编码器模式下 CH1/CH2 都配置为输入, 分别从 TI1/TI2 引脚取信号 */
  sEncoderConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sEncoderConfig.IC1Polarity = (ENC1_INVERT_DIR != 0U) ? TIM_ENCODERINPUTPOLARITY_FALLING
                                                       : TIM_ENCODERINPUTPOLARITY_RISING;
  sEncoderConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;   /* CH1 直连 TI1(A相) */
  sEncoderConfig.IC1Prescaler = TIM_ICPSC_DIV1;             /* 每个有效边沿都计数 */
  sEncoderConfig.IC1Filter = ENC_INPUT_FILTER;

  sEncoderConfig.IC2Polarity = sEncoderConfig.IC1Polarity;
  sEncoderConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;   /* CH2 直连 TI2(B相) */
  sEncoderConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sEncoderConfig.IC2Filter = ENC_INPUT_FILTER;

  HAL_TIM_Encoder_Init(&htim1, &sEncoderConfig);
  __HAL_TIM_SET_COUNTER(&htim1, 0);
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);               /* 启动 CH1/CH2 输入与计数器 */

  /* ---------------- 编码器2: TIM5, PA0(A相)/PA1(B相) ---------------- */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 65535;                             /* 与编码器1统一按 16 位量程, 由软件累加成 32 位 */
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  /* 编码器2 的完整配置独立赋值, 不依赖编码器1 留下的字段 */
  sEncoderConfig.IC1Polarity = (ENC2_INVERT_DIR != 0U) ? TIM_ENCODERINPUTPOLARITY_FALLING
                                                       : TIM_ENCODERINPUTPOLARITY_RISING;
  sEncoderConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sEncoderConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sEncoderConfig.IC1Filter = ENC_INPUT_FILTER;

  sEncoderConfig.IC2Polarity = sEncoderConfig.IC1Polarity;
  sEncoderConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sEncoderConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sEncoderConfig.IC2Filter = ENC_INPUT_FILTER;

  HAL_TIM_Encoder_Init(&htim5, &sEncoderConfig);

  __HAL_TIM_SET_COUNTER(&htim5, 0);
  HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);

  /* 零点对齐: 先读一次当前原始值, 再清空累计量。
     这样第一次 Encoder_Update() 得到的是"从初始化到第一次采样"的真实增量,
     而不是把硬件计数器的上电随机值当成一次巨大的跳变。 */
  s_last_raw_enc1 = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
  s_last_raw_enc2 = (uint16_t)__HAL_TIM_GET_COUNTER(&htim5);
  g_enc1_count = 0;
  g_enc2_count = 0;
  g_enc1_delta = 0;
  g_enc2_delta = 0;
}

/**
  *@brief 周期采样编码器: 读硬件计数器, 算出本次增量并累加到 32 位位置
  *       必须在固定周期(建议 10ms)的中断或任务里调用, 例如放在 TIM2 中断中:
  *
  *           void TIM2_IRQHandler(void)
  *           {
  *             HAL_TIM_IRQHandler(&htim2);
  *             Encoder_Update();
  *           }
  *
  *       注意: 本函数内部用 int16_t 相减, 正确的前提是"两次调用之间
  *       计数变化不超过 ±32767"; 采样周期越短越安全, 10ms 足够。
  *@param: None
  *@return None
  **/
void Encoder_Update(void)
{
  uint16_t now1 = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
  uint16_t now2 = (uint16_t)__HAL_TIM_GET_COUNTER(&htim5);

  /* 无符号相减再解释为有符号 16 位: 计数器跨 0/65535 回绕时结果依然正确 */
  g_enc1_delta = (int16_t)(now1 - s_last_raw_enc1);
  g_enc2_delta = (int16_t)(now2 - s_last_raw_enc2);

  s_last_raw_enc1 = now1;
  s_last_raw_enc2 = now2;

  /* 累加成 32 位位置: 只要单周期不丢步就不会回绕 */
  g_enc1_count += g_enc1_delta;
  g_enc2_count += g_enc2_delta;
}

/**
  *@brief 读取累计位置(有符号, 32 位, 单调增减不回绕)
  *@param enc: ENC_1 或 ENC_2
  *@return 累计脉冲数, 正数 = 正转; 需要时除以(每圈脉冲数 × 4)换算成圈数
  **/
int32_t Encoder_GetCount(uint8_t enc)
{
  if (enc == ENC_1)
  {
    return g_enc1_count;
  }
  else if (enc == ENC_2)
  {
    return g_enc2_count;
  }
  return 0;
}

/**
  *@brief 读取最近一个采样周期的脉冲增量(有符号)
  *       速度 = 增量 / (采样周期 × 每圈脉冲数 × 4)
  *@param enc: ENC_1 或 ENC_2
  *@return 本周期增量: >0 正转, <0 反转, =0 未转动
  **/
int32_t Encoder_GetDelta(uint8_t enc)
{
  if (enc == ENC_1)
  {
    return g_enc1_delta;
  }
  else if (enc == ENC_2)
  {
    return g_enc2_delta;
  }
  return 0;
}

/**
  *@brief 读取硬件计数器的原始瞬时值(0~65535)
  *       【仅用于调试/自检】: 它是自由回绕的硬件值, 没有零点含义,
  *       拿它做控制或显示会看到乱跳的锯齿, 正常业务请用 Encoder_GetCount()
  *@param enc: ENC_1 或 ENC_2
  *@return 该编码器定时器的 CNT 寄存器当前值
  **/
int32_t Encoder_GetRawCounter(uint8_t enc)
{
  if (enc == ENC_1)
  {
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim1);
  }
  else if (enc == ENC_2)
  {
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim5);
  }
  return 0;
}

/**
  *@brief 编码器清零: 把当前位置记为新的零点
  *       同时清掉硬件计数器、累计位置、周期增量和内部上次采样值, 保持一致
  *@param enc: ENC_1 或 ENC_2
  *@return None
  **/
void Encoder_Reset(uint8_t enc)
{
  if (enc == ENC_1)
  {
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    s_last_raw_enc1 = 0;
    g_enc1_delta = 0;
    g_enc1_count = 0;
  }
  else if (enc == ENC_2)
  {
    __HAL_TIM_SET_COUNTER(&htim5, 0);
    s_last_raw_enc2 = 0;
    g_enc2_delta = 0;
    g_enc2_count = 0;
  }
}

/**
  *@brief 驱动模块硬件初始化入口(MSP)
  *       负责电机方向IO,输出PWM的引脚使能
  *@param: None
  *@return None
  **/
void Drving_msp_Init(void)
{
  /*方向IO初始化  AIN1->PE11  AIN2->PE12 BIN1->PE13 BIN2->PE14  */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOE_CLK_ENABLE();
  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode =GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 8399;   /* 84MHz/(8399+1)=10kHz, 周期99 -> 100Hz */
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 99;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_OC_Init(&htim3);
  HAL_TIM_PWM_Init(&htim3);
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_OC_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3);
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4);
  HAL_TIM_MspPostInit(&htim3);

  HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_3 );
  HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_4 );
}

/**
*@brief:PWMA输出函数
*@param Duty:PWMA的占空比,>0时正转,<0时反转
*@return None
**/
void PWM_A_OUT(int8_t Duty)
{
  if(Duty <= 100 && Duty >= -100)//占空比限幅
  {
    if(Duty > 0)  //占空比大于0正转
    {
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4,Duty);
    }
    else    //占空比小于0反转
    {
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4,-Duty);
    }
  }  
}


/**
*@brief:PWMB输出函数
*@param Duty:PWMB的占空比,>0时正转,<0时反转
*@return None
**/
void PWM_B_OUT(int8_t Duty)
{
 if(Duty<= 100 && Duty>= -100)//占空比限幅
  {
    if(Duty > 0)  //占空比大于0正转
    {
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_RESET);
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3,Duty);
    }
    else    //占空比小于0反转
    {
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3,-Duty);
    }
  }  
}

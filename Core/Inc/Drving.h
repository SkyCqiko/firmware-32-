#ifndef __DRVING_H
#define __DRVING_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/*==============================================================================
 *  驱动模块(Drving)
 *  ---------------------------------------------------------------------------
 *  编码器接口(正交编码器模式):
 *    编码器1(TIM1) : PE9  = TIM1_CH1 = A相,  PE10 = TIM1_CH2 = B相
 *    编码器2(TIM5) : PA0  = TIM5_CH1 = A相,  PA1  = TIM5_CH2 = B相
 *  ---------------------------------------------------------------------------
 *  计数方式 : TIM_ENCODERMODE_TI12 (四倍频: A/B 两相的上升沿和下降沿都计数)
 *  计数范围 : 两个编码器的硬件计数器都按 16 位量程(0~65535)配置,
 *             由 Encoder_Update() 在溢出前转成 32 位累计位置, 永不回绕。
 *  ---------------------------------------------------------------------------
 *  【三个取值函数的区别, 这是本模块最容易用错的地方】
 *
 *    Encoder_GetCount(enc)     累计位置, 32 位有符号, 单调增减永不回绕
 *                              -> 用于计算走了多远 / 转了多少圈
 *
 *    Encoder_GetDelta(enc)     最近一个采样周期的脉冲增量, 有符号
 *                              -> 用于计算速度: 转速 = 增量 / (采样周期 × 每圈脉冲数)
 *
 *    Encoder_GetRawCounter(enc) 硬件计数器的原始瞬时值(0~65535)
 *                              -> 仅用于调试/自检, 会自由回绕, 不要拿它做控制
 *
 *  【关键】后两个函数以及累计位置的更新, 都依赖 Encoder_Update() 被周期调用,
 *          调用周期必须固定(建议放定时器中断里, 例如 10ms 一次):
 *
 *              void TIM2_IRQHandler(void)
 *              {
 *                HAL_TIM_IRQHandler(&htim2);
 *                Encoder_Update();      // 固定周期采样, 只读寄存器, 极短
 *              }
 *
 *          若 Encoder_Update() 没有被调用, Encoder_GetCount/GetDelta 会一直返回旧值。
 *  ---------------------------------------------------------------------------
 *  方向约定 : 返回值为正 = 车轮正转。若实测方向相反, 两种改法任选其一:
 *               1. 把该编码器的 A/B 两相接线对调;
 *               2. 在 Drving.c 中把 ENC1_INVERT_DIR / ENC2_INVERT_DIR 改成 1。
 *  ---------------------------------------------------------------------------
 *  速度换算示例(以每圈 13 线编码器、四倍频、10ms 采样为例):
 *              rps = Encoder_GetDelta(ENC_1) / (0.01f * 13.0f * 4.0f)
 *============================================================================*/

/* 编码器编号(传给各取值函数) */
#define ENC_1   1U   /* TIM1: PE9/PE10 */
#define ENC_2   2U   /* TIM5: PA0/PA1  */

/* TIM1/TIM5 句柄, 供中断函数或其它文件引用 */
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim5;

/* 累计位置(32 位有符号), 由 Encoder_Update() 维护;
   在中断里更新、主循环里读取, 所以必须是 volatile */
extern volatile int32_t g_enc1_count;
extern volatile int32_t g_enc2_count;

/* 最近一个采样周期的脉冲增量, 由 Encoder_Update() 维护 */
extern volatile int32_t g_enc1_delta;
extern volatile int32_t g_enc2_delta;

/*-------------------------------- 初始化 ---------------------------------*/
void Drving_msp_Init(void);   /* 电机方向 IO + PWM 初始化 */
void Encoder_Init(void);      /* 编码器接口初始化 */

/*------------------------------ 采样与取值 -------------------------------*/
/**
  *@brief 周期采样编码器: 读硬件计数器, 算出本次增量并累加到位置
  *       必须在固定周期(建议 10ms)的中断或任务里调用
  *       函数本身极短, 只读两个寄存器并做一次减法, 适合放中断
  *@param: None
  *@return None
  **/
void Encoder_Update(void);

int32_t Encoder_GetCount(uint8_t enc);       /* 累计位置(32位, 不回绕) */
int32_t Encoder_GetDelta(uint8_t enc);       /* 最近一个周期的增量     */
int32_t Encoder_GetRawCounter(uint8_t enc);  /* 原始计数器(仅调试用)   */
void    Encoder_Reset(uint8_t enc);          /* 清零: 位置/增量/硬件计数器 */

/*-------------------------------- 电机 PWM -------------------------------*/
void PWM_A_OUT(int8_t Duty);   /* Duty: -100~100, 正=正转 负=反转 */
void PWM_B_OUT(int8_t Duty);

#endif

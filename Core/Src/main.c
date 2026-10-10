/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  * 主控：STM32F407VET6
  * 定时中断：
  * 显示屏：8针脚SPI驱动TFT彩屏(硬件SPI3)
  * 摄像头传感器：OV2640摄像头模块(SCCB驱动，DCMI接收)
  * 舵机驱动：TIM4的通道1(PC12引脚)
  * 电机驱动：TB6612模块(PWMA = PB1,PWMB = PB0)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "TFT.h"
#include "TFT_Config.h"
#include "OV_camera.h"
#include "Image.h"
#include "Servo.h"
#include "Key.h"
#include "timer.h"
#include "Drving.h"

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"
#include <stdint.h>

/*---------------------------------------------------------------------------*/

/*  变量声明   */
#define IMG_W 120
#define IMG_H 92

uint16_t i;
float Path_error;//路径误差
static uint8_t Keynum =1;         //按键
static uint8_t Stop_flag =0;      //停止标志位
int32_t Encoder_Data = 0;  //编码器读取

extern uint32_t Image[DEAL_IMAGE_W*DEAL_IMAGE_H/2];      /* 图像缓存区*/
extern uint8_t Gray_Image[DEAL_IMAGE_H][DEAL_IMAGE_W];
extern uint8_t Binarization_Image[DEAL_IMAGE_H][DEAL_IMAGE_W];

extern TIM_HandleTypeDef htim2;
void SystemClock_Config(void);
/*===============================*     主函数     *===========================================*/

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  Lcd_Init();             /* TFT LCD Init*/
  Lcd_Clear(BLACK);//绘制背景
  OV2640_Init();          /*OV2640 Camera Init*/
  Key_Init();             /*模式转换开关初始化*/
  Servo_Init();          /*舵机初始化*/
  Timer_Init();         /*普通定时器初始化: TIM2, 10ms 定时中断(初始化封装在 timer.c 中)*/
  Encoder_Init();       /*编码器接口初始化: 编码器1 = TIM1(PE9/PE10), 编码器2 = TIM5(PA0/PA1)*/
  Drving_msp_Init();    /*电机IO及PWM初始化*/

  while (1)
  {
    Path_error = Image_Process(20,0 );
    if(Keynum == 1){PWM_A_OUT(30);PWM_B_OUT(30);Stop_flag = 0;}
    if(Keynum == 2){PWM_A_OUT(-30);PWM_B_OUT(-30);Stop_flag = 0;}
    if(Keynum == 3)
    {
      if( Stop_flag==0){PWM_A_OUT(50);PWM_B_OUT(50);}
      else {PWM_A_OUT(0);PWM_B_OUT(0);}
    }
     Encoder_Data = Encoder_GetDelta(ENC_1);
    Lcd_DisplayImage((uint16_t *)Image,IMG_W,IMG_H);
  }
}
/*=================================================================================================*/

/*================*   定时器中断服务函数   *====================*/
/**
  * @brief  TIM2 定时器中断服务函数(10ms 进入一次)
  *         初始化在 timer.c 的 Timer_Init() 中完成, 中断函数放在 main.c。
  *         HAL_TIM_IRQHandler() 会判断并清除更新中断标志, 然后回调
  *         HAL_TIM_PeriodElapsedCallback()(若未实现该回调则只清标志)。
  * @param  None
  * @retval None
  */
void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim2);
  /* ---- 每 10ms 执行一次的代码写在这里 ---- */
    Keynum = Key_readBit();  //按键读取
  Encoder_Reset(ENC_1);
    Encoder_Update();
  g_tim2_tick++;             /* 10ms 计数, 主循环里可用它做非阻塞延时 */
  if(g_tim2_tick%2000==0)Stop_flag = 1;
}
/*===========================================================*/








/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* HSI(16MHz) -> PLL -> 168MHz。
     原来只有 HSI 直通 16MHz: DCMI+DMA 带宽上限约 8MByte/s,
     而 OV2640 的 PCLK 有 12~24MHz(RGB565 数据率约 24MByte/s), 一定溢出(OVR)。
     另外 TIM3/TIM4 的预分频原本是按 72MHz 以上写的, 16MHz 下电机 PWM 只有 22Hz、舵机只有 11Hz。*/
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;                 /* 16MHz/16 = 1MHz VCO 输入 */
  RCC_OscInitStruct.PLL.PLLN = 336;                /* VCO = 336MHz */
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;      /* SYSCLK = 168MHz */
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;    /* HCLK  = 168MHz */
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;     /* PCLK1 = 42MHz, 定时器时钟 84MHz */
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;     /* PCLK2 = 84MHz */

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}
/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */

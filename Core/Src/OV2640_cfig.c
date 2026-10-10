#include "main.h"
#include "stm32f407xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_dcmi.h"
#include "OV_camera.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_gpio.h"

DMA_HandleTypeDef dma;
DCMI_HandleTypeDef hdcmi;
volatile uint8_t frameReady = 0;

/* SCCB 自检结果: 1 = OV2640 通信正常(PID 读到 0x26/0x42), 0 = 通信失败 */
volatile uint8_t ov2640_id_ok = 0;

/* SCCB 写寄存器时没收到应答(NACK)的次数。正常应始终为 0;
   不为 0 说明有寄存器没真正写进去(会表现为画面偏色/参数不对), 最常见原因是位延时太短。 */
volatile uint32_t ov2640_sccb_nack_count = 0;

/* 等一帧完成的超时(ms)。一帧约 33ms(30fps); 暗光下自动曝光会把帧率压到几 fps,
   所以留足余量。只在真正出错时才会等这么久。 */
#define OV_FRAME_TIMEOUT_MS  500U


/* ---------- SCCB 位延时 ----------
   原来每个位都用 HAL_Delay(1)(实际 1~2ms), 一次寄存器写有 58 个位延时 = 60~120ms,
   250+ 次寄存器写下来初始化要十几秒 —— 表现就是"开机蓝屏十几秒才出画面"。
   这里改用 DWT 周期计数器做精确的微秒级延时(与编译优化等级无关), 约 25kHz 总线速率,
   SCCB 初始化总计约 0.15 秒。

   为什么取 20us 而不是更快: SCCB 是开漏 + 上拉, 上升沿靠上拉电阻给线容充电。
   走线长、上拉偏弱时上升沿可能要好几微秒, 位延时太短会让寄存器写静默失败(画面偏色、
   参数不对), 而写操作本身不会报错。20us 半周期留了足够余量。
   若确认 ov2640_sccb_nack_count 始终为 0, 可把 OV_SCCB_DELAY_US 调小(最小约 3,
   对应约 160kHz)来进一步缩短初始化时间。
   注意: PWDN/复位那种毫秒级时序仍然保留 HAL_Delay()。 */
#define OV_SCCB_DELAY_US  20U

static void OV_SCCB_DelayInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* 使能内核跟踪单元 */
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;              /* 启动周期计数器 */
}

static void OV_SCCB_Delay(void)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = (SystemCoreClock / 1000000U) * OV_SCCB_DELAY_US;

  while ((DWT->CYCCNT - start) < ticks)
  {
  }
}

/**
  * @brief OV2640 GPIO初始化函数
  * @retval None
  */
void OV_GPIO_Init(void)
{
   GPIO_InitTypeDef GPIO_InitStruct = {0};

  OV_SCCB_DelayInit();          /* 启动 DWT 周期计数器, 供 SCCB 的微秒级位延时使用 */

  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();      /* DCMI 用到 PA4(HSYNC)/PA6(PIXCLK), 需要 GPIOA 时钟 */

  /*通用IO初始化 : OV_SCL_Pin OV_SDA_Pin */
  GPIO_InitStruct.Pin = OV_SCL_Pin|OV_SDA_Pin;
  GPIO_InitStruct.Mode =GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PB4 为 .ioc 中声明的 OV_PWDN; PB9 是旧代码写的位置, 原理图确认前一并拉低做兼容 */
  GPIO_InitStruct.Pin = OV_PWDN_Pin|OV_RST_Pin|OV_PWDN_ALT_Pin;
  GPIO_InitStruct.Mode =GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OV_PWDN_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(OV_PWDN_GPIO_Port, OV_PWDN_Pin|OV_PWDN_ALT_Pin, GPIO_PIN_RESET); /* PWDN 低 = 正常工作 */
  HAL_GPIO_WritePin(OV_RST_GPIO_Port, OV_RST_Pin, GPIO_PIN_RESET);                   /* RST  低 = 复位状态 */
  __HAL_RCC_DMA2_CLK_ENABLE();
  dma.Instance = DMA2_Stream1;
  dma.Init.Channel = DMA_CHANNEL_1;
  dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
  dma.Init.PeriphInc = DMA_PINC_DISABLE;
  dma.Init.MemInc = DMA_MINC_ENABLE;
  dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  dma.Init.Mode = DMA_CIRCULAR;   
  dma.Init.Priority = DMA_PRIORITY_HIGH;
  dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&dma) != HAL_OK)
  {
    Error_Handler();      
  }

 /*DCMI接口初始化*/
  hdcmi.Instance = DCMI;
  hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
  hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_RISING;
  hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_LOW;    
  hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;    
  hdcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;
  hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
  hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;
  __HAL_LINKDMA(&hdcmi, DMA_Handle, dma);
  if (HAL_DCMI_Init(&hdcmi) != HAL_OK)
  {
    Error_Handler();
  }

  /* 中断使能 */
  HAL_NVIC_SetPriority(DCMI_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DCMI_IRQn);
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
}
/* DCMI 中断: 帧完成 / 行 / 同步错误 / 溢出 */
void DCMI_IRQHandler(void) { HAL_DCMI_IRQHandler(&hdcmi); }
void DMA2_Stream1_IRQHandler(void) { HAL_DMA_IRQHandler(&dma); }

/**
  * @brief SCCB 开启函数
  * @retval None
  */
void OV_SCCB_Start(void)
{
 HAL_GPIO_WritePin(OV_SDA_GPIO_Port, OV_SDA_Pin, GPIO_PIN_SET);
 HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_SET);
 OV_SCCB_Delay();
 HAL_GPIO_WritePin(OV_SDA_GPIO_Port, OV_SDA_Pin, GPIO_PIN_RESET);
 OV_SCCB_Delay();
 HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_RESET);
}

/**
  * @brief SCCB 终止函数
  * @retval None
  */
void OV_SCCB_Stop(void)
{
  HAL_GPIO_WritePin(OV_SDA_GPIO_Port, OV_SDA_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_RESET);
  OV_SCCB_Delay();
  HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_SET);
  OV_SCCB_Delay();
  HAL_GPIO_WritePin(OV_SDA_GPIO_Port, OV_SDA_Pin, GPIO_PIN_SET);
}

/**
  * @brief SCCB 发送一个字节, 并读从机的应答位
  * @param Data: 发送的数据
  * @retval 1: 收到应答(ACK, 从机把 SDA 拉低)   0: 无应答(NACK)
  */
uint8_t OV_SCCB_WriteByte(uint8_t Data)
{
  uint8_t j, res, ack;

  for(j=0;j<8;j++)
  {
    res = (Data & 0x80U) ? 1U : 0U;
    HAL_GPIO_WritePin(OV_SDA_GPIO_Port, OV_SDA_Pin, res);
    Data <<= 1;
    OV_SCCB_Delay();
    HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_SET);
    OV_SCCB_Delay();
    HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_RESET);
  }

  /* 第 9 位: 主机必须先释放 SDA, 再拉高 SCL, 然后读从机有没有把 SDA 拉低(低 = 应答) */
  HAL_GPIO_WritePin(OV_SDA_GPIO_Port, OV_SDA_Pin, GPIO_PIN_SET);
  OV_SCCB_Delay();
  HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_SET);
  OV_SCCB_Delay();
  ack = (HAL_GPIO_ReadPin(OV_SDA_GPIO_Port, OV_SDA_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
  HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_RESET);
  OV_SCCB_Delay();

  return ack;
}

/**
  * @brief SCCB 接收数据函数
  * @retval Data 接收到的数据
  */
uint8_t OV_SCCB_RDByte(void)
{
  uint8_t j,Data=0;
  HAL_GPIO_WritePin(OV_SDA_GPIO_Port, OV_SDA_Pin, GPIO_PIN_SET);
  for(j=0;j<8;j++)
  {
    Data<<=1;
    OV_SCCB_Delay();
    HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_SET);
    OV_SCCB_Delay();        /* SCL 拉高后等 SDA 稳定再采样, 否则可能读错位 */
    if(HAL_GPIO_ReadPin(OV_SDA_GPIO_Port, OV_SDA_Pin))
      Data|=0x01;
    OV_SCCB_Delay();
    HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_RESET);
  }
  return Data;
}

/**
  * @brief SCCB 写寄存器
  * @param reg: 寄存器地址
  * @param Data: 发送的数据
  * @retval None
  */
/**
  * @brief SCCB 写寄存器(带应答检查: 任何一段没收到 ACK 都会累加 ov2640_sccb_nack_count)
  * @param reg: 寄存器地址
  * @param Data: 发送的数据
  * @retval None
  */
void OV_SCCB_WriteReg(uint8_t reg,uint8_t Data)
{
  uint8_t ack = 1U;

  OV_SCCB_Start();
  ack &= OV_SCCB_WriteByte(OV2640_WRITE_ADDR);
  ack &= OV_SCCB_WriteByte(reg);
  ack &= OV_SCCB_WriteByte(Data);
  OV_SCCB_Stop();

  if (ack == 0U)
  {
    ov2640_sccb_nack_count++;    /* 这次写没被应答: 多半是位延时太短或接线问题 */
  }
}

/**
  * @brief SCCB 主机非应答(第9位: 释放 SDA 保持高电平)
  * @retval None
  */
void OV_SCCB_NoAck(void)
{
  HAL_GPIO_WritePin(OV_SDA_GPIO_Port, OV_SDA_Pin, GPIO_PIN_SET);
  OV_SCCB_Delay();
  HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_SET);
  OV_SCCB_Delay();
  HAL_GPIO_WritePin(OV_SCL_GPIO_Port, OV_SCL_Pin, GPIO_PIN_RESET);
  OV_SCCB_Delay();
}

/**
  * @brief SCCB 读寄存器
  * @param reg: 寄存器地址
  * @retval 读到的数据
  */
uint8_t OV_SCCB_ReadReg(uint8_t reg)
{
  uint8_t Data;

  /* 第一段: 只写寄存器地址 */
  OV_SCCB_Start();
  OV_SCCB_WriteByte(OV2640_WRITE_ADDR);
  OV_SCCB_WriteByte(reg);
  OV_SCCB_Stop();

  /* 第二段: 发读地址, 读一个字节, 然后主机非应答 */
  OV_SCCB_Start();
  OV_SCCB_WriteByte(OV2640_READ_ADDR);
  Data = OV_SCCB_RDByte();
  OV_SCCB_NoAck();
  OV_SCCB_Stop();

  return Data;
}

/**
  * @brief 读取并校验 OV2640 的器件 ID(sensor 寄存器组 0x0A/0x0B, 应为 0x26/0x42)
  * @retval 1: SCCB 通信正常  0: 通信失败
  */
uint8_t OV2640_CheckID(void)
{
  uint8_t pidh, pidl;

  OV_SCCB_WriteReg(OV2640_DSP_RA_DLMT, 0x01);          /* 切到 sensor 寄存器组 */
  pidh = OV_SCCB_ReadReg(OV2640_SENSOR_PIDH);          /* 0x0A */
  pidl = OV_SCCB_ReadReg(OV2640_SENSOR_PIDL);          /* 0x0B */

  return ((pidh == 0x26U) && (pidl == 0x42U)) ? 1U : 0U;
}


/**
  * @brief SCCB 色度设置函数
  * @param sat: 色度值(0->-2 , 1->-1 , 2->0 , 3->+1 , 4->+2)
  * @retval None
  */
void OV2640_Color_Saturation(uint8_t sat)
{ 
	uint8_t reg7dval=((sat+2)<<4)|0X08;
	OV_SCCB_WriteReg(0XFF,0X00);		
	OV_SCCB_WriteReg(0X7C,0X00);		
	OV_SCCB_WriteReg(0X7D,0X02);				
	OV_SCCB_WriteReg(0X7C,0X03);			
	OV_SCCB_WriteReg(0X7D,reg7dval);			
	OV_SCCB_WriteReg(0X7D,reg7dval); 		
}

/**
  * @brief SCCB 亮度设置函数
  * @param sat: 亮度值(0:(0X00)-2 1:(0X10)-1 2,(0X20) 0 3,(0X30)+1 4,(0X40)+2)
  * @retval None
  */
void OV2640_Brightness(uint8_t bright)
{
  OV_SCCB_WriteReg(0xff, 0x00);
  OV_SCCB_WriteReg(0x7c, 0x00);
  OV_SCCB_WriteReg(0x7d, 0x04);
  OV_SCCB_WriteReg(0x7c, 0x09);
  OV_SCCB_WriteReg(0x7d, bright<<4);
  OV_SCCB_WriteReg(0x7d, 0x00);
}


/**
  * @brief SCCB 对比度设置函数
  * @param contrast: 对比度值(0:(0X18)-2 1:(0X1C)-1 2,(0X20) 0 3,(0X24)+1 4,(0X28)+2)
  * @retval None
  */
void OV2640_Contrast(uint8_t contrast)
{
	uint8_t reg7d0val=0X20;//默认为普通模式
	uint8_t reg7d1val=0X20;
  	switch(contrast)
	{
		case 0://-2
			reg7d0val=0X18;	 	 
			reg7d1val=0X34;	 	 
			break;	
		case 1://-1
			reg7d0val=0X1C;	 	 
			reg7d1val=0X2A;	 	 
			break;	
		case 3://1
			reg7d0val=0X24;	 	 
			reg7d1val=0X16;	 	 
			break;	
		case 4://2
			reg7d0val=0X28;	 	 
			reg7d1val=0X0C;	 	 
			break;	
	}
	OV_SCCB_WriteReg(0xff,0x00);
	OV_SCCB_WriteReg(0x7c,0x00);
	OV_SCCB_WriteReg(0x7d,0x04);
	OV_SCCB_WriteReg(0x7c,0x07);
	OV_SCCB_WriteReg(0x7d,0x20);
	OV_SCCB_WriteReg(0x7d,reg7d0val);
	OV_SCCB_WriteReg(0x7d,reg7d1val);
	OV_SCCB_WriteReg(0x7d,0x06);
}

/**
  * @brief SCCB 特殊效果设置函数
  * @param eft: 效果值(0:普通 1:负片 2:黑白 3:偏红色 4:偏绿色 5:偏蓝色 6:复古)
  * @retval None
  */
void OV2640_Special_Effects(uint8_t eft)
{
	uint8_t reg7d0val=0X00;//默认为普通模式
	uint8_t reg7d1val=0X80;
	uint8_t reg7d2val=0X80; 
	switch(eft)
	{
		case 1://负片
			reg7d0val=0X40; 
			break;	
		case 2://黑白
			reg7d0val=0X18; 
			break;	 
		case 3://偏红色
			reg7d0val=0X18; 
			reg7d1val=0X40;
			reg7d2val=0XC0; 
			break;	
		case 4://偏绿色
			reg7d0val=0X18; 
			reg7d1val=0X40;
			reg7d2val=0X40; 
			break;	
		case 5://偏蓝色
			reg7d0val=0X18; 
			reg7d1val=0XA0;
			reg7d2val=0X40; 
			break;	
		case 6://复古
			reg7d0val=0X18; 
			reg7d1val=0X40;
			reg7d2val=0XA6; 
			break;	 
	}
	OV_SCCB_WriteReg(0xff,0x00);
	OV_SCCB_WriteReg(0x7c,0x00);
	OV_SCCB_WriteReg(0x7d,reg7d0val);
	OV_SCCB_WriteReg(0x7c,0x05);
	OV_SCCB_WriteReg(0x7d,reg7d1val);
	OV_SCCB_WriteReg(0x7d,reg7d2val); 
}

/**
  * @brief SCCB 自动曝光设置函数
  * @param level: 曝光等级(0~4)
  * @retval None
  */
void OV2640_Auto_Exposure(uint8_t level)
{  
	uint8_t i;
	uint8_t *p=(uint8_t*)OV2640_AUTOEXPOSURE_LEVEL[level];
	for(i=0;i<4;i++)
	{ 
		OV_SCCB_WriteReg(p[i*2],p[i*2+1]); 
	} 
}  

/**
  * @brief OV2640 输出图像大小设置函数
  * @param width: 图像宽度
  * @param height: 图像高度
  * @retval None
  */
uint8_t OV2640_OutSize_Set(uint16_t width,uint16_t height)
{
	uint16_t outh;
	uint16_t outw;
	uint8_t temp; 
	if(width%4)return 1;
	if(height%4)return 2;
	outw=width/4;
	outh=height/4; 
	OV_SCCB_WriteReg(0XFF,0X00);	
	OV_SCCB_WriteReg(0XE0,0X04);			
	OV_SCCB_WriteReg(0X5A,outw&0XFF);		//设置OUTW的低八位
	OV_SCCB_WriteReg(0X5B,outh&0XFF);		//设置OUTH的低八位
	temp=(outw>>8)&0X03;
	temp|=(outh>>6)&0X04;
	OV_SCCB_WriteReg(0X5C,temp);				//设置OUTH/OUTW的高位 
	OV_SCCB_WriteReg(0XE0,0X00);	
	return 0;
}

/**
  * @brief OV2640 获取图像数据函数
  * @param Image_p: 图像数据指针
  * @param Length: 数据长度
  * @retval None
  */
void OV_GetImageData(uint32_t Image_p, uint32_t Length)
{
  uint32_t tickstart;

  frameReady = 0;

  /* CONTINUOUS 模式 + 循环 DMA:
     DCMI 在每帧结束时置 FRAME 标志, HAL_DCMI_FrameEventCallback() 把 frameReady 置 1;
     循环 DMA 保证缓冲区搬满后 DMA 不会自我关闭(否则帧尾那几个像素没人取会触发 OVR 溢出)。
     注意同步极性必须与摄像头匹配: OV2640 实测需要 PCK=Rising / VS=LOW / HS=LOW,
     配错时 DCMI 会把 HREF 有效窗口之外的数据也采进来(一帧数据量会膨胀到十几倍)。 */
  if (HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, Image_p, Length) != HAL_OK)
  {
    return;
  }

  /* 等一帧完成, 带超时保护: 摄像头没输出时不会死等 */
  tickstart = HAL_GetTick();
  while (frameReady == 0)
  {
    if ((HAL_GetTick() - tickstart) > OV_FRAME_TIMEOUT_MS)
    {
      break;
    }
  }

  HAL_DCMI_Stop(&hdcmi);
  frameReady = 0;
}

void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi_cb)
{
  (void)hdcmi_cb;
  frameReady = 1;         /* 设置帧采集完成标志 */
}


/**
  * @brief OV2640 初始化函数
  * @retval None
  */
void OV2640_Init(void)
{
	uint8_t i;
	OV_GPIO_Init();//GPIO/DCMI/DMA初始化
	HAL_GPIO_WritePin(OV_PWDN_GPIO_Port, OV_PWDN_Pin|OV_PWDN_ALT_Pin, GPIO_PIN_RESET);//PWDN拉低 = 正常工作
	HAL_Delay(10);
	HAL_GPIO_WritePin(OV_RST_GPIO_Port, OV_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(OV_RST_GPIO_Port, OV_RST_Pin, GPIO_PIN_SET);//OV2640硬件复位
	HAL_Delay(10);

	/* SCCB 自检: 先读回 OV2640 的 PID(0x0A/0x0B 应为 0x26/0x42)。
	   若 ov2640_id_ok 始终为 0, 说明 SCCB 通信失败, 后面所有寄存器配置都不会生效;
	   此时应先查 PB3(SCL)/PB5(SDA) 接线与上拉, 而不是怀疑 DCMI。*/
	ov2640_id_ok = OV2640_CheckID();

	OV_SCCB_WriteReg(OV2640_DSP_RA_DLMT, 0x01);	//操作sensor寄存器
 	OV_SCCB_WriteReg(OV2640_SENSOR_COM7, 0x80);	//软复位OV2640
	HAL_Delay(10);

	/* ★ 循环次数用"行数"= sizeof(表)/sizeof(表[0])。
	   注意两个表都必须逐行 {reg,val} 初始化(见 OV_camera.h):
	   - RGB565 表原来是 14 个字节的扁平序列, 编译器把它当成 [7][2],
	     循环只跑 7 次且整体错位一个字节, 0xDA 从未在 bank 0 被写成 0x09;
	   - SVGA 表是扁平的 388 字节, 恰好凑成 194 行, 原写法碰巧没出错,
	     但那是"字节数/2 == 行数"的巧合, 表一变就会少写。
	   现在两个表都按行显式初始化, 次数按元素大小算, 不再依赖巧合。 */
	for(i=0;i<(sizeof(ov2640_svga_init_reg_tbl)/sizeof(ov2640_svga_init_reg_tbl[0]));i++)
	{
	   	OV_SCCB_WriteReg(ov2640_svga_init_reg_tbl[i][0],ov2640_svga_init_reg_tbl[i][1]);//初始化分辨率为SVGA--800*600
 	} 
	for(i=0;i<(sizeof(ov2640_rgb565_reg_tbl)/sizeof(ov2640_rgb565_reg_tbl[0]));i++)
	{OV_SCCB_WriteReg(ov2640_rgb565_reg_tbl[i][0],ov2640_rgb565_reg_tbl[i][1]);} //RGB565格式 
	
	OV_SCCB_WriteReg(0XFF,0X00);	 
	OV_SCCB_WriteReg(0XC7,0X00);//自动白平衡
	
	OV2640_Color_Saturation(2);
	OV2640_Brightness(2);
	OV2640_Contrast(2);
	OV2640_Auto_Exposure(2);
 // OV2640_Special_Effects(2);     //灰白特效
	
	OV2640_OutSize_Set(120,92); //输出 120*92 (宽高必须是4的倍数, 90 不满足), 与采集长度 120*92/2 对应
}

#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_spi.h"
#include <stdint.h>
#include "TFT_Config.h"
#include "TFT_Font.h"


 SPI_HandleTypeDef hspi3;

/**
  * @brief TFT屏GPIO初始化
  * @param None
  * @retval None
  */
void TFT_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();          //开启GPIOA时钟
    __HAL_RCC_GPIOC_CLK_ENABLE();          //开启GPIOC时钟

    /*Configure GPIO pins : TFT_BLK_Pin TFT_RST_Pin TFT_CS_Pin */
    GPIO_InitStruct.Pin = TFT_BLK_Pin|TFT_RST_Pin|TFT_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : TFT_SCL_Pin TFT_SDA_Pin*/
    GPIO_InitStruct.Pin = TFT_SCL_Pin|TFT_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

     /*Configure GPIO pin : TFT_DC_Pin */
    GPIO_InitStruct.Pin = TFT_DC_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TFT_DC_GPIO_Port, &GPIO_InitStruct);

	 HAL_GPIO_WritePin(GPIOA, TFT_RST_Pin|TFT_CS_Pin, GPIO_PIN_RESET); 
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_BLK_GPIO_Port, TFT_BLK_Pin, GPIO_PIN_SET);
}

/**
  * @brief TFT屏SPI初始化(对应F407的SPI3)
  * @param None
  * @retval None
  */
void TFT_SPI_Init(void)
{
    hspi3.Instance = SPI3;
    hspi3.Init.Mode = SPI_MODE_MASTER;
    hspi3.Init.Direction = SPI_DIRECTION_1LINE;
    hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi3.Init.NSS = SPI_NSS_SOFT;
    /* SPI3 时钟源为 PCLK1(42MHz), 分频 4 -> 10.5MHz SCLK, 超过 ST7735 上限会花屏 */
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi3.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi3);
}

/*SPI发送8位数据*/
void Lcd_WriteData(uint8_t Data)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(&hspi3, &Data, 1, 0);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

}

/*SPI发送8位指令*/
void Lcd_WriteIndex(uint8_t Index)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi3, &Index, 1, 0);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

/*SPI发送16位数据*/
void LCD_WriteData_16Bit(uint16_t Data)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);

    uint8_t high = (uint8_t)(Data >> 8);
    uint8_t low  = (uint8_t)Data;

    HAL_SPI_Transmit(&hspi3, &high, 1, 0);
    HAL_SPI_Transmit(&hspi3, &low, 1, 0);

    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

/*SPI配置寄存器*/
void Lcd_WriteReg(uint8_t Index,uint8_t Data)
{
    Lcd_WriteIndex(Index);
    Lcd_WriteData(Data);
}

/*TFT屏硬件复位*/
void TFT_Reset(void)
{
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port,TFT_RST_Pin,GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(50);
}



/**
  * @brief TFT屏初始化
  * @param None
  * @retval None
  */
void Lcd_Init(void)
{
    TFT_GPIO_Init();
    TFT_SPI_Init();
    TFT_Reset(); //Reset before LCD Init.

	//LCD Init For 1.44Inch LCD Panel with ST7735R.
	Lcd_WriteIndex(0x11);//Sleep exit 
	HAL_Delay(120);
		
	//ST7735R Frame Rate
	Lcd_WriteIndex(0xB1); 
	Lcd_WriteData(0x01); 
	Lcd_WriteData(0x2C); 
	Lcd_WriteData(0x2D); 

	Lcd_WriteIndex(0xB2); 
	Lcd_WriteData(0x01); 
	Lcd_WriteData(0x2C); 
	Lcd_WriteData(0x2D); 

	Lcd_WriteIndex(0xB3); 
	Lcd_WriteData(0x01); 
	Lcd_WriteData(0x2C); 
	Lcd_WriteData(0x2D); 
	Lcd_WriteData(0x01); 
	Lcd_WriteData(0x2C); 
	Lcd_WriteData(0x2D); 
	
	Lcd_WriteIndex(0xB4); //Column inversion 
	Lcd_WriteData(0x07); 
	
	//ST7735R Power Sequence
	Lcd_WriteIndex(0xC0); 
	Lcd_WriteData(0xA2); 
	Lcd_WriteData(0x02); 
	Lcd_WriteData(0x84); 
	Lcd_WriteIndex(0xC1); 
	Lcd_WriteData(0xC5); 

	Lcd_WriteIndex(0xC2); 
	Lcd_WriteData(0x0A); 
	Lcd_WriteData(0x00); 

	Lcd_WriteIndex(0xC3); 
	Lcd_WriteData(0x8A); 
	Lcd_WriteData(0x2A); 
	Lcd_WriteIndex(0xC4); 
	Lcd_WriteData(0x8A); 
	Lcd_WriteData(0xEE); 
	
	Lcd_WriteIndex(0xC5); //VCOM 
	Lcd_WriteData(0x0E); 
	
	Lcd_WriteIndex(0x36); //MX, MY, RGB mode 
	Lcd_WriteData(0xC0); 
	
	//ST7735R Gamma Sequence
	Lcd_WriteIndex(0xe0); 
	Lcd_WriteData(0x0f); 
	Lcd_WriteData(0x1a); 
	Lcd_WriteData(0x0f); 
	Lcd_WriteData(0x18); 
	Lcd_WriteData(0x2f); 
	Lcd_WriteData(0x28); 
	Lcd_WriteData(0x20); 
	Lcd_WriteData(0x22); 
	Lcd_WriteData(0x1f); 
	Lcd_WriteData(0x1b); 
	Lcd_WriteData(0x23); 
	Lcd_WriteData(0x37); 
	Lcd_WriteData(0x00); 	
	Lcd_WriteData(0x07); 
	Lcd_WriteData(0x02); 
	Lcd_WriteData(0x10); 

	Lcd_WriteIndex(0xe1); 
	Lcd_WriteData(0x0f); 
	Lcd_WriteData(0x1b); 
	Lcd_WriteData(0x0f); 
	Lcd_WriteData(0x17); 
	Lcd_WriteData(0x33); 
	Lcd_WriteData(0x2c); 
	Lcd_WriteData(0x29); 
	Lcd_WriteData(0x2e); 
	Lcd_WriteData(0x30); 
	Lcd_WriteData(0x30); 
	Lcd_WriteData(0x39); 
	Lcd_WriteData(0x3f); 
	Lcd_WriteData(0x00); 
	Lcd_WriteData(0x07); 
	Lcd_WriteData(0x03); 
	Lcd_WriteData(0x10);  
	
	Lcd_WriteIndex(0x2a);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x7f);

	Lcd_WriteIndex(0x2b);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x00);
	Lcd_WriteData(0x9f);
	
	Lcd_WriteIndex(0xF0); //Enable test command  
	Lcd_WriteData(0x01); 
	Lcd_WriteIndex(0xF6); //Disable ram power save mode 
	Lcd_WriteData(0x00); 
	
	Lcd_WriteIndex(0x3A); //65k mode 
	Lcd_WriteData(0x05); 
	
	
	Lcd_WriteIndex(0x29);//Display on	 
}

/**
  * @brief TFT屏设置显示区域
  * @param x_start x轴起始坐标，最大128
  * @param y_start y轴起始坐标，最大160
  * @param x_end x轴结束坐标，最大128
  * @param y_end y轴结束坐标，最大160
  * @retval None
  */
void Lcd_SetRegion(uint8_t x_start,uint8_t y_start,uint8_t x_end,uint8_t y_end)
{		
	/* 列/行的起始与结束必须加相同的面板偏移(LCD_X_OFFSET/LCD_Y_OFFSET)。
	   原代码是 x_start 不加、x_end+2, 窗口宽度变成 122, 而每行只写 120 个像素,
	   ST7735 在窗口右边界提前换行 -> 每行向左错 2 像素、越往下越偏(斜切)。*/
	Lcd_WriteIndex(0x2a);
	Lcd_WriteData(0x00);
	Lcd_WriteData(x_start + LCD_X_OFFSET);
	Lcd_WriteData(0x00);
	Lcd_WriteData(x_end   + LCD_X_OFFSET);

	Lcd_WriteIndex(0x2b);
	Lcd_WriteData(0x00);
	Lcd_WriteData(y_start + LCD_Y_OFFSET);
	Lcd_WriteData(0x00);
	Lcd_WriteData(y_end   + LCD_Y_OFFSET);
	
	Lcd_WriteIndex(0x2c);
}

/**
  * @brief TFT屏上绘制一点
  * @param x x轴坐标，最大128
  * @param y y轴坐标，最大160
  * @param ColorData 绘制点的颜色数据
  * @retval None
  */
void Gui_DrawPoint(uint8_t x, uint8_t y, uint16_t ColorData)
{
    Lcd_SetRegion(x, y, x + 1, y + 1);
    LCD_WriteData_16Bit(ColorData);
}

/**
  * @brief 清屏函数
  * @param Color 屏幕颜色 
  * @retval None
  */
void Lcd_Clear(uint16_t Color)               
{	
   unsigned int i,m;
   Lcd_SetRegion(0,0,X_MAX_PIXEL-1,Y_MAX_PIXEL-1);
   for(i=0;i<Y_MAX_PIXEL;i++)
    for(m=0;m<X_MAX_PIXEL;m++)
    {	
	  	LCD_WriteData_16Bit(Color);
    }   
}

/**
  * @brief TFT屏显示图像函数(一维数组数据)
  * @param image 一维图像数据指针
  * @param width 图像宽度，最大128
  * @param height 图像高度，最大160
  * @retval None
  */
void Lcd_DisplayImage(uint16_t *image, uint16_t width, uint16_t height)
{
	uint32_t i;
	Lcd_SetRegion(0, 0, width -1, height - 1);
	Lcd_WriteIndex(0x2C);		//写入内存数据
	for (i = 0; i < width * height; i++)
	{
		LCD_WriteData_16Bit(image[i]);
	}
	HAL_Delay(5);
}

/**
  * @brief TFT屏显示灰度图像函数(二维数组数据)
  * @param image 一维图像数据指针
  * @param width 图像宽度，最大128
  * @param height 图像高度，最大160
  * @retval None
  */
void Lcd_DisplayImage_Gray_2(uint8_t *image, uint16_t width, uint16_t height)
{
	
	uint32_t i,j;
	if (image == NULL || width == 0U || height == 0U)
	{
		return;
	}
	Lcd_SetRegion(0, 0, width - 1U, height - 1U);
	Lcd_WriteIndex(0x2C);		//写入内存数据
	for(j=0;j<height;j++)
	for (i = 0; i < width; i++)
	{
		uint8_t  g = *(image+j*width+i);
		uint16_t rgb565;
		rgb565 = (uint16_t)(((uint16_t)(g & 0xF8U) << 8)      //R: bit7~3 -> bit15~11
						  | ((uint16_t)(g & 0xFCU) << 3)      //G: bit7~2 -> bit10~5
						  | ((uint16_t)g >> 3));              //B: bit7~3 -> bit4~0

		LCD_WriteData_16Bit(rgb565);
	}
	HAL_Delay(5);
}

/**
  * @brief TFT屏显示二值化图像函数
  * @param image 二维图像数据指针
  * @param width 图像宽度，最大128
  * @param height 图像高度，最大160
  * @retval None
  */
void Lcd_DisplayImage_Binary(uint8_t *image, uint16_t width, uint16_t height)
{
	uint32_t i,j;
	Lcd_SetRegion(0, 0, width -1, height - 1);
	Lcd_WriteIndex(0x2C);		//写入内存数据
	for(j=0;j<height;j++)
	for (i = 0; i < width; i++)
	{
		if(*(image+j*width+i) == 0)LCD_WriteData_16Bit(BLACK);  //暗处为黑色
		else if(*(image+j*width+i) == 1){LCD_WriteData_16Bit(WHITE);}    //亮处为白色
		else if (*(image+j*width+i) == 2) {LCD_WriteData_16Bit(GREEN);}  //2左线为绿色
		else if (*(image+j*width+i) == 3) {LCD_WriteData_16Bit(YELLOW);}  //3中线为黄色
		else if (*(image+j*width+i) == 4) {LCD_WriteData_16Bit(RED);} 	 //4右线为红色
	}
	HAL_Delay(5);
}


/**
  * @brief TFT绘制圆形函数
  * @param X:圆心所在x轴
  * @param Y:圆心所在y轴
  * @param R:半径
  * @param Fc:边框颜色
  * @retval None
  */
void Gui_Circle(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc) 
{//Bresenham算法 
    unsigned short  a,b; 
    int c; 
    a = 0 ; 
    b = R ; 
    c = 3 - 2 * R; 
    while (a<b) 
    { 
        Gui_DrawPoint(X+a,Y+b,fc);     //        7 
        Gui_DrawPoint(X-a,Y+b,fc);     //        6 
        Gui_DrawPoint(X+a,Y-b,fc);     //        2 
        Gui_DrawPoint(X-a,Y-b,fc);     //        3 
        Gui_DrawPoint(X+b,Y+a,fc);     //        8 
        Gui_DrawPoint(X-b,Y+a,fc);     //        5 
        Gui_DrawPoint(X+b,Y-a,fc);     //        1 
        Gui_DrawPoint(X-b,Y-a,fc);     //        4 
        if(c<0) c=c+4*a+6; 
        else 
        { 
            c=c+4*(a-b)+10; 
            b-=1; 
        } 
       a+=1; 
    } 
    if (a==b) 
    { 
        Gui_DrawPoint(X+a,Y+b,fc); 
        Gui_DrawPoint(X+a,Y+b,fc); 
        Gui_DrawPoint(X+a,Y-b,fc); 
        Gui_DrawPoint(X-a,Y-b,fc); 
        Gui_DrawPoint(X+b,Y+a,fc); 
        Gui_DrawPoint(X-b,Y+a,fc); 
        Gui_DrawPoint(X+b,Y-a,fc); 
        Gui_DrawPoint(X-b,Y-a,fc); 
    } 
	
} 


/** 
* @brief 画线函数，使用Bresenham 画线算法
* @param x0:起始点X轴坐标
* @param y0:起始点y轴坐标
* @param x1:终止点X轴坐标
* @param y1:终止点y轴坐标
* @param Color:线的颜色
**/
void Gui_DrawLine(uint16_t x0, uint16_t y0,uint16_t x1, uint16_t y1,uint16_t Color)   
{
int dx,             // difference in x's
    dy,             // difference in y's
    dx2,            // dx,dy * 2
    dy2, 
    x_inc,          // amount in pixel space to move during drawing
    y_inc,          // amount in pixel space to move during drawing
    error,          // the discriminant i.e. error i.e. decision variable
    index;          // used for looping	
	Lcd_SetRegion(x0,y0,x0,y0);
	dx = x1-x0;//计算x距离
	dy = y1-y0;//计算y距离
	if (dx>=0)
	{
		x_inc = 1;
	}
	else
	{
		x_inc = -1;
		dx    = -dx;  
	} 
	if (dy>=0)
	{
		y_inc = 1;
	} 
	else
	{
		y_inc = -1;
		dy    = -dy; 
	} 
	dx2 = dx << 1;
	dy2 = dy << 1;
	if (dx > dy)//x距离大于y距离，那么每个x轴上只有一个点，每个y轴上有若干个点
	{//且线的点数等于x距离，以x轴递增画点
		// initialize error term
		error = dy2 - dx; 
		// draw the line
		for (index=0; index <= dx; index++)//要画的点数不会超过x距离
		{
			//画点
			Gui_DrawPoint(x0,y0,Color);
			// test if error has overflowed
			if (error >= 0) //是否需要增加y坐标值
			{
				error-=dx2;
				// move to next line
				y0+=y_inc;//增加y坐标值
			} // end if error overflowed
			// adjust the error term
			error+=dy2;
			// move to the next pixel
			x0+=x_inc;//x坐标值每次画点后都递增1
		} // end for
	} // end if |slope| <= 1
	else//y轴大于x轴，则每个y轴上只有一个点，x轴若干个点
	{//以y轴为递增画点
		// initialize error term
		error = dx2 - dy; 
		// draw the line
		for (index=0; index <= dy; index++)
		{
			// set the pixel
			Gui_DrawPoint(x0,y0,Color);
			// test if error overflowed
			if (error >= 0)
			{
				error-=dy2;
				// move to next line
				x0+=x_inc;
			} // end if error overflowed
			// adjust the error term
			error+=dx2;
			// move to the next pixel
			y0+=y_inc;
		} // end for
	} // end else |slope| > 1
}

/** 
* @brief 字符显示函数
* @param x0:X轴坐标
* @param y0:y轴坐标
* @param fc:外部颜色大小
* @param bc:内部颜色大小
* @param s:字符串
**/
void Gui_DrawFont_GBK16(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s)
{
	unsigned char i,j;
	unsigned short k,x0;
	x0=x;

	while(*s) 
	{	
		if((*s) < 128) 
		{
			k=*s;
			if (k==13) 
			{
				x=x0;
				y+=16;
			}
			else 
			{
				if (k>32) k-=32; else k=0;
	
			    for(i=0;i<16;i++)
				for(j=0;j<8;j++) 
					{
				    	if(asc16[k*16+i]&(0x80>>j))	Gui_DrawPoint(x+j,y+i,fc);
						else 
						{
							if (fc!=bc) Gui_DrawPoint(x+j,y+i,bc);
						}
					}
				x+=8;
			}
			s++;
		}
	}
}

/** 
* @brief 有符号整数变量显示函数(定宽刷新, 不留残影)
* @param x:X轴坐标(显示区左上角)
* @param y:Y轴坐标
* @param fc:字体颜色
* @param bc:背景颜色(fc!=bc 时才会把上一帧的数字描掉)
* @param num:要显示的变量, 支持负数
* @param width:数字部分固定占用的字符个数(不含符号), 不足时左侧补空格
* @note 与 Gui_DrawFont_GBK16 同一套架构: 只用 ASCII 字模, 不调用 printf。
*       每个字符固定占 8x16 像素, 所以在同一个 x/y 上周期刷新时,
*       旧内容会被背景色整格覆盖, 不会出现残影。
*       符号位固定占最左一格(正数显示为空格), 这样变量在正负之间
*       变化时数字不会左右跳动。
**/
void Gui_DrawNum(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, int32_t num, uint8_t width)
{
	uint8_t tmp[10];			/* 逆序暂存各位数字 */
	uint8_t buf[16];			/* 符号 1 格 + 最多 10 位数字 + 结束符 */
	uint8_t pos=0,digits=0,pad,i;
	uint32_t mag;

	if (width>10u) width=10u;		/* int32_t 最多 10 位数字 */

	if (num<0)
	{
		buf[pos++]='-';
		mag=0u-(uint32_t)num;		/* 无符号取负, INT32_MIN 也能正确取到幅度 */
	}
	else
	{
		buf[pos++]=' ';			/* 正数用空格占住符号位 */
		mag=(uint32_t)num;
	}

	do					/* 先逆序取出每一位 */
	{
		tmp[digits++]=(uint8_t)('0'+(uint8_t)(mag%10u));
		mag/=10u;
	} while ((mag!=0u)&&(digits<10u));

	pad=(digits<width)?(uint8_t)(width-digits):0u;	/* 不足 width 位就补0, 右对齐 */
	while (pad!=0u)
	{
		buf[pos++]='0';
		pad--;
	}

	i=digits;				/* 再倒过来放进缓冲区 */
	while (i!=0u)
	{
		i--;
		buf[pos++]=tmp[i];
	}
	buf[pos]=0;

	Gui_DrawFont_GBK16(x,y,fc,bc,buf);
}

/** 
* @brief 浮点变量显示函数(拆成整数部分 + 小数部分, 不用 printf)
* @param x:X轴坐标(显示区左上角)
* @param y:Y轴坐标
* @param fc:字体颜色
* @param bc:背景颜色
* @param val:要显示的变量
* @param int_width:整数部分固定字符个数(不含符号, 符号固定占 1 格)
* @param frac_width:小数位数(0~6), 为 0 时按整数显示
* @note 先用 10^frac_width 把数值缩放到整数, 再复用 Gui_DrawNum,
*       整数和小数都走同一套 8x16 字模, 显示风格统一。
*       建议 |val| < 21474836, 否则缩放后会溢出 int32_t。
**/
void Gui_DrawFloat(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, float val, uint8_t int_width, uint8_t frac_width)
{
	uint8_t buf[10];
	uint32_t scale=1u;
	int32_t scaled,ipart,fpart;
	uint8_t i;

	if (frac_width>6u) frac_width=6u;
	for (i=0u;i<frac_width;i++) scale*=10u;

	scaled=(int32_t)((val*(float)scale)+((val>=0.0f)?0.5f:-0.5f));	/* 四舍五入 */

	ipart=scaled/(int32_t)scale;		/* C99 除法向 0 取整: -1.5 -> -1 */
	fpart=scaled%(int32_t)scale;		/* 余数与被除数同号 */
	if (fpart<0) fpart=-fpart;

	Gui_DrawNum(x,y,fc,bc,ipart,int_width);

	if ((scaled<0)&&(ipart==0))
	{
		/* |val|<1 时整数部分取整后是 0, Gui_DrawNum 会把它当正数,
		   这里把符号格重画成负号, 保证显示 "-0.05" 而不是 " 0.05" */
		Gui_DrawFont_GBK16(x,y,fc,bc,(uint8_t *)"-");
	}

	if (frac_width==0u) return;

	buf[0]='.';
	for (i=0u;i<frac_width;i++)
	{
		buf[frac_width-i]=(uint8_t)('0'+(uint8_t)((uint32_t)fpart%10u));
		fpart/=10;
	}
	buf[frac_width+1]=0;

	/* 符号位 1 格 + 整数 int_width 格之后就是小数点 */
	Gui_DrawFont_GBK16((uint16_t)(x+(int_width+1)*8),y,fc,bc,buf);
}

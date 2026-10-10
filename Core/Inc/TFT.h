#ifndef __TFT_H
#define __TFT_H

#include <stdint.h>

void TFT_GPIO_Init(void);
void TFT_SPI_Init(void);
void TFT_Reset(void);
void Lcd_Init(void);
void Lcd_SetRegion(uint8_t x_start,uint8_t y_start,uint8_t x_end,uint8_t y_end);
void Gui_DrawPoint(uint8_t x, uint8_t y, uint16_t ColorData);

void Lcd_Clear(uint16_t Color);
void Lcd_DisplayImage(uint16_t *image, uint16_t width, uint16_t height);
void Lcd_DisplayImage_Gray_2(uint8_t *image, uint16_t width, uint16_t height);
void Lcd_DisplayImage_Binary(uint8_t *image, uint16_t width, uint16_t height);

void Gui_Circle(uint16_t X,uint16_t Y,uint16_t R,uint16_t fc);
void Gui_DrawLine(uint16_t x0, uint16_t y0,uint16_t x1, uint16_t y1,uint16_t Color);

/* 字符 / 变量显示: 8x16 ASCII 字模, 不依赖 printf */
void Gui_DrawFont_GBK16(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s);
void Gui_DrawNum(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, int32_t num, uint8_t width);
void Gui_DrawFloat(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, float val, uint8_t int_width, uint8_t frac_width);

#endif
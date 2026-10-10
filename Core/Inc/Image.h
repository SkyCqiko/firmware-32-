#ifndef __IMAGE_H
#define __IMAGE_H

#include <stdint.h>

#define DEAL_IMAGE_H 92    //定义处理图像的高度
#define DEAL_IMAGE_W 120    //定义处理图像的宽度

/* 一维灰度数组 -> 二维数组(按值逐行拷贝)。
   参数顺序为(源, 目标, width, height), 与 main.c 中的调用一致。
   注意: 原声明写成 uint8_t *Image_2[][DEAL_IMAGE_W](即"指针的二维数组"),
   与 Image.c 中的定义不是同一类型, 两个原型冲突, 已一并修正为
   "指向 DEAL_IMAGE_W 元素数组的指针", 这样 TransImage(Image_gray, Image_gray_2, ...)
   这种传二维数组名的调用才能正确匹配。 */
void TransImage(uint8_t *Image_1, uint8_t (*Image_2)[DEAL_IMAGE_W], uint16_t width, uint16_t Highth);
void Image_RGB_to_Gray(uint16_t *Image_p, uint32_t Length,uint8_t *Image_Gray_p);
uint8_t Image_Binary(uint8_t *in_image,uint8_t *out_image);
void image_draw_rectan(unsigned char(*image)[DEAL_IMAGE_W]);
void Image_Filter(unsigned char *in_image);
void Search_Line(unsigned char(*image)[DEAL_IMAGE_W]);
float Line_Curvature(unsigned char in_line[DEAL_IMAGE_H],unsigned char len,unsigned char start,unsigned char end);
float Image_Process(unsigned char prospect, float error_offset);
#endif
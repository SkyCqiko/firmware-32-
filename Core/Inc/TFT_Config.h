#ifndef TFT_CONFIG_H
#define TFT_CONFIG_H

#define X_MAX_PIXEL 128    //X轴最大值
#define Y_MAX_PIXEL 160    //Y轴最大值

/* ST7735 的 GRAM 实际是 132x162, 128x160 的可见区域从 (2,1) 开始, 所以列/行都要加这个偏移。
   关键: 起始和结束必须加【相同】的偏移 —— 否则窗口宽度会等于 (end-start+1+偏移差),
   和每行写入的像素数对不上, ST7735 会在窗口右边界提前换行, 画面就会斜切错位。
   如果整幅画面整体偏了(不是斜切), 只微调这两个值即可。*/
#define LCD_X_OFFSET 2
#define LCD_Y_OFFSET 1

#define RED  	0xf800          //红色
#define GREEN	0x0fef          //绿色
#define BLUE 	0x001f          //蓝色
#define WHITE	0xffff          //白色
#define BLACK	0x0000          //黑色
#define YELLOW  0xFFE0          //黄色
#define GRAY0   0xEF7D   	    //灰色0 
#define GRAY1   0x8410      	//灰色1
#define GRAY2   0x4208      	//灰色2

#endif
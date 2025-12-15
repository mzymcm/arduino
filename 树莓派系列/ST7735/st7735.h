#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>

// 屏幕尺寸
#define ST7735_WIDTH   128
#define ST7735_HEIGHT  128

// 颜色定义 (RGB565)
#define BLACK     0x0000
#define BLUE      0x001F
#define RED       0xF800
#define GREEN     0x07E0
#define CYAN      0x07FF
#define MAGENTA   0xF81F
#define YELLOW    0xFFE0
#define WHITE     0xFFFF

// 函数声明
void ST7735_Init(void);
void ST7735_WriteCommand(uint8_t cmd);
void ST7735_WriteData(uint8_t data);
void ST7735_WriteData16(uint16_t data);
void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void ST7735_FillScreen(uint16_t color);
void ST7735_DrawPixel(uint8_t x, uint8_t y, uint16_t color);
void ST7735_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color);
void ST7735_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
void ST7735_FillRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
void ST7735_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint16_t color);
void ST7735_DrawChar(uint8_t x, uint8_t y, char ch, uint16_t color, uint16_t bg);
void ST7735_DrawString(uint8_t x, uint8_t y, const char* str, uint16_t color, uint16_t bg);
void ST7735_Backlight(uint8_t state);
void ST7735_Cleanup(void);

#endif

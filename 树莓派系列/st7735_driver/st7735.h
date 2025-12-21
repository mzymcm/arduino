#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>
#include <stdbool.h>

// 显示屏尺寸（横屏）
#define ST7735_WIDTH    160
#define ST7735_HEIGHT   128

// 颜色定义 (RGB565)
#define ST7735_BLACK    0x0000
#define ST7735_BLUE     0x001F
#define ST7735_RED      0xF800
#define ST7735_GREEN    0x07E0
#define ST7735_CYAN     0x07FF
#define ST7735_MAGENTA  0xF81F
#define ST7735_YELLOW   0xFFE0
#define ST7735_WHITE    0xFFFF

// 旋转方向
typedef enum {
    ST7735_ROTATION_0 = 0,
    ST7735_ROTATION_90,
    ST7735_ROTATION_180,
    ST7735_ROTATION_270
} st7735_rotation_t;

// ST7735设备结构体
typedef struct {
    uint16_t width;
    uint16_t height;
    st7735_rotation_t rotation;
    uint16_t *framebuffer;
} st7735_t;

// 初始化函数
int st7735_init(st7735_t *dev, st7735_rotation_t rotation);
void st7735_deinit(st7735_t *dev);

// 基本绘图函数
void st7735_clear(st7735_t *dev, uint16_t color);
void st7735_set_pixel(st7735_t *dev, uint16_t x, uint16_t y, uint16_t color);
void st7735_draw_rect(st7735_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7735_fill_rect(st7735_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7735_draw_line(st7735_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void st7735_draw_circle(st7735_t *dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void st7735_fill_circle(st7735_t *dev, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);

// 显示控制
void st7735_update(st7735_t *dev);
void st7735_set_backlight(bool state);
void st7735_set_rotation(st7735_t *dev, st7735_rotation_t rotation);

// 文本绘制
void st7735_draw_char(st7735_t *dev, char ch, uint16_t x, uint16_t y, 
                      uint16_t color, uint16_t bg_color, uint8_t size);
void st7735_draw_string(st7735_t *dev, const char *str, uint16_t x, uint16_t y,
                       uint16_t color, uint16_t bg_color, uint8_t size);

// 工具函数
uint16_t st7735_color_rgb(uint8_t r, uint8_t g, uint8_t b);

#endif // ST7735_H
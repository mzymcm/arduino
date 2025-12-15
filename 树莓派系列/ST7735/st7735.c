#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bcm2835.h>
#include "st7735.h"

// 引脚定义 (BCM编号)
#define DC_PIN      RPI_V2_GPIO_P1_22  // GPIO25
#define RST_PIN     RPI_V2_GPIO_P1_13  // GPIO27
#define BL_PIN      RPI_V2_GPIO_P1_18  // GPIO24
#define CS_PIN      RPI_V2_GPIO_P1_24  // GPIO8 (CE0)

// ST7735命令定义
#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID   0x04
#define ST7735_RDDST   0x09
#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_RAMRD   0x2E
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36

// 全局变量
static uint16_t _width = ST7735_WIDTH;
static uint16_t _height = ST7735_HEIGHT;
static uint8_t _colstart = 0;
static uint8_t _rowstart = 0;

// 简单字体数据 (5x7字体)
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 空格
    {0x00,0x00,0x5F,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00}, // "
    {0x14,0x7F,0x14,0x7F,0x14}, // #
    {0x24,0x2A,0x7F,0x2A,0x12}, // $
    {0x23,0x13,0x08,0x64,0x62}, // %
    {0x36,0x49,0x55,0x22,0x50}, // &
    {0x00,0x05,0x03,0x00,0x00}, // '
    {0x00,0x1C,0x22,0x41,0x00}, // (
    {0x00,0x41,0x22,0x1C,0x00}, // )
    {0x08,0x2A,0x1C,0x2A,0x08}, // *
    {0x08,0x08,0x3E,0x08,0x08}, // +
    {0x00,0x50,0x30,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08}, // -
    {0x00,0x60,0x60,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02}, // /
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00}, // ;
    {0x00,0x08,0x14,0x22,0x41}, // <
    {0x14,0x14,0x14,0x14,0x14}, // =
    {0x41,0x22,0x14,0x08,0x00}, // >
    {0x02,0x01,0x51,0x09,0x06}, // ?
    {0x32,0x49,0x79,0x41,0x3E}, // @
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x01,0x01}, // F
    {0x3E,0x41,0x41,0x51,0x32}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x04,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x03,0x04,0x78,0x04,0x03}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
    {0x00,0x00,0x7F,0x41,0x41}, // [
    {0x02,0x04,0x08,0x10,0x20}, // backslash
    {0x41,0x41,0x7F,0x00,0x00}, // ]
    {0x04,0x02,0x01,0x02,0x04}, // ^
    {0x40,0x40,0x40,0x40,0x40}, // _
    {0x00,0x01,0x02,0x04,0x00}, // `
    {0x20,0x54,0x54,0x54,0x78}, // a
    {0x7F,0x48,0x44,0x44,0x38}, // b
    {0x38,0x44,0x44,0x44,0x20}, // c
    {0x38,0x44,0x44,0x48,0x7F}, // d
    {0x38,0x54,0x54,0x54,0x18}, // e
    {0x08,0x7E,0x09,0x01,0x02}, // f
    {0x08,0x14,0x54,0x54,0x3C}, // g
    {0x7F,0x08,0x04,0x04,0x78}, // h
    {0x00,0x44,0x7D,0x40,0x00}, // i
    {0x20,0x40,0x44,0x3D,0x00}, // j
    {0x7F,0x10,0x28,0x44,0x00}, // k
    {0x00,0x41,0x7F,0x40,0x00}, // l
    {0x7C,0x04,0x18,0x04,0x78}, // m
    {0x7C,0x08,0x04,0x04,0x78}, // n
    {0x38,0x44,0x44,0x44,0x38}, // o
    {0x7C,0x14,0x14,0x14,0x08}, // p
    {0x08,0x14,0x14,0x18,0x7C}, // q
    {0x7C,0x08,0x04,0x04,0x08}, // r
    {0x48,0x54,0x54,0x54,0x20}, // s
    {0x04,0x3F,0x44,0x40,0x20}, // t
    {0x3C,0x40,0x40,0x20,0x7C}, // u
    {0x1C,0x20,0x40,0x20,0x1C}, // v
    {0x3C,0x40,0x30,0x40,0x3C}, // w
    {0x44,0x28,0x10,0x28,0x44}, // x
    {0x0C,0x50,0x50,0x50,0x3C}, // y
    {0x44,0x64,0x54,0x4C,0x44}, // z
    {0x00,0x08,0x36,0x41,0x00}, // {
    {0x00,0x00,0x7F,0x00,0x00}, // |
    {0x00,0x41,0x36,0x08,0x00}, // }
    {0x08,0x04,0x08,0x10,0x08}, // ~
};

// 写命令
void ST7735_WriteCommand(uint8_t cmd) {
    bcm2835_gpio_write(DC_PIN, LOW);  // DC低电平表示命令
    bcm2835_spi_transfer(cmd);
}

// 写数据
void ST7735_WriteData(uint8_t data) {
    bcm2835_gpio_write(DC_PIN, HIGH); // DC高电平表示数据
    bcm2835_spi_transfer(data);
}

// 写16位数据
void ST7735_WriteData16(uint16_t data) {
    bcm2835_gpio_write(DC_PIN, HIGH);
    bcm2835_spi_transfer(data >> 8);
    bcm2835_spi_transfer(data & 0xFF);
}

// 设置地址窗口
void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    // 设置列地址
    ST7735_WriteCommand(ST7735_CASET);
    ST7735_WriteData(0x00);
    ST7735_WriteData(x0 + _colstart);
    ST7735_WriteData(0x00);
    ST7735_WriteData(x1 + _colstart);
    
    // 设置行地址
    ST7735_WriteCommand(ST7735_RASET);
    ST7735_WriteData(0x00);
    ST7735_WriteData(y0 + _rowstart);
    ST7735_WriteData(0x00);
    ST7735_WriteData(y1 + _rowstart);
    
    // 写入RAM
    ST7735_WriteCommand(ST7735_RAMWR);
}

// 初始化ST7735
void ST7735_Init(void) {
    // 初始化GPIO
    bcm2835_gpio_fsel(DC_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(RST_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(BL_PIN, BCM2835_GPIO_FSEL_OUTP);
    
    // 初始化SPI
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64); // ~4MHz
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    
    // 硬件复位
    bcm2835_gpio_write(RST_PIN, HIGH);
    bcm2835_delay(100);
    bcm2835_gpio_write(RST_PIN, LOW);
    bcm2835_delay(100);
    bcm2835_gpio_write(RST_PIN, HIGH);
    bcm2835_delay(100);
    
    // 背光开启
    ST7735_Backlight(1);
    
    // 软件复位
    ST7735_WriteCommand(ST7735_SWRESET);
    bcm2835_delay(150);
    
    // 退出睡眠模式
    ST7735_WriteCommand(ST7735_SLPOUT);
    bcm2835_delay(150);
    
    // 帧率控制
    ST7735_WriteCommand(0xB1);
    ST7735_WriteData(0x01);
    ST7735_WriteData(0x2C);
    ST7735_WriteData(0x2D);
    
    ST7735_WriteCommand(0xB2);
    ST7735_WriteData(0x01);
    ST7735_WriteData(0x2C);
    ST7735_WriteData(0x2D);
    
    ST7735_WriteCommand(0xB3);
    ST7735_WriteData(0x01);
    ST7735_WriteData(0x2C);
    ST7735_WriteData(0x2D);
    ST7735_WriteData(0x01);
    ST7735_WriteData(0x2C);
    ST7735_WriteData(0x2D);
    
    // 逆变控制
    ST7735_WriteCommand(0xB4);
    ST7735_WriteData(0x07);
    
    // 电源控制
    ST7735_WriteCommand(0xC0);
    ST7735_WriteData(0xA2);
    ST7735_WriteData(0x02);
    ST7735_WriteData(0x84);
    
    ST7735_WriteCommand(0xC1);
    ST7735_WriteData(0xC5);
    
    ST7735_WriteCommand(0xC2);
    ST7735_WriteData(0x0A);
    ST7735_WriteData(0x00);
    
    ST7735_WriteCommand(0xC3);
    ST7735_WriteData(0x8A);
    ST7735_WriteData(0x2A);
    
    ST7735_WriteCommand(0xC4);
    ST7735_WriteData(0x8A);
    ST7735_WriteData(0xEE);
    
    ST7735_WriteCommand(0xC5);
    ST7735_WriteData(0x0E);
    
    // VCOM控制
    ST7735_WriteCommand(0x36);
    ST7735_WriteData(0xC0); // 设置方向
    
    // 颜色格式
    ST7735_WriteCommand(ST7735_COLMOD);
    ST7735_WriteData(0x05); // RGB565格式
    
    // 伽马校正
    ST7735_WriteCommand(0xE0);
    ST7735_WriteData(0x02);
    ST7735_WriteData(0x1C);
    ST7735_WriteData(0x07);
    ST7735_WriteData(0x12);
    ST7735_WriteData(0x37);
    ST7735_WriteData(0x32);
    ST7735_WriteData(0x29);
    ST7735_WriteData(0x2D);
    ST7735_WriteData(0x29);
    ST7735_WriteData(0x25);
    ST7735_WriteData(0x2B);
    ST7735_WriteData(0x39);
    ST7735_WriteData(0x00);
    ST7735_WriteData(0x01);
    ST7735_WriteData(0x03);
    ST7735_WriteData(0x10);
    
    ST7735_WriteCommand(0xE1);
    ST7735_WriteData(0x03);
    ST7735_WriteData(0x1D);
    ST7735_WriteData(0x07);
    ST7735_WriteData(0x06);
    ST7735_WriteData(0x2E);
    ST7735_WriteData(0x2C);
    ST7735_WriteData(0x29);
    ST7735_WriteData(0x2D);
    ST7735_WriteData(0x2E);
    ST7735_WriteData(0x2E);
    ST7735_WriteData(0x37);
    ST7735_WriteData(0x3F);
    ST7735_WriteData(0x00);
    ST7735_WriteData(0x00);
    ST7735_WriteData(0x02);
    ST7735_WriteData(0x10);
    
    // 正常显示开启
    ST7735_WriteCommand(ST7735_NORON);
    bcm2835_delay(10);
    
    // 显示开启
    ST7735_WriteCommand(ST7735_DISPON);
    bcm2835_delay(100);
    
    _colstart = 2;
    _rowstart = 3;
}

// 填充整个屏幕
void ST7735_FillScreen(uint16_t color) {
    ST7735_SetAddressWindow(0, 0, _width - 1, _height - 1);
    
    for(uint32_t i = 0; i < _width * _height; i++) {
        ST7735_WriteData16(color);
    }
}

// 绘制单个像素
void ST7735_DrawPixel(uint8_t x, uint8_t y, uint16_t color) {
    if((x >= _width) || (y >= _height)) return;
    
    ST7735_SetAddressWindow(x, y, x, y);
    ST7735_WriteData16(color);
}

// 绘制直线 (Bresenham算法)
void ST7735_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    while(1) {
        ST7735_DrawPixel(x0, y0, color);
        if((x0 == x1) && (y0 == y1)) break;
        int16_t e2 = 2 * err;
        if(e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if(e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// 绘制矩形
void ST7735_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    ST7735_DrawLine(x, y, x + w - 1, y, color);          // 上边
    ST7735_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color);  // 下边
    ST7735_DrawLine(x, y, x, y + h - 1, color);          // 左边
    ST7735_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);  // 右边
}

// 绘制填充矩形
void ST7735_FillRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    if((x >= _width) || (y >= _height)) return;
    if((x + w - 1) >= _width) w = _width - x;
    if((y + h - 1) >= _height) h = _height - y;
    
    ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    
    for(uint32_t i = 0; i < w * h; i++) {
        ST7735_WriteData16(color);
    }
}

// 绘制圆形 (中点圆算法)
void ST7735_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    ST7735_DrawPixel(x0, y0 + r, color);
    ST7735_DrawPixel(x0, y0 - r, color);
    ST7735_DrawPixel(x0 + r, y0, color);
    ST7735_DrawPixel(x0 - r, y0, color);
    
    while(x < y) {
        if(f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        ST7735_DrawPixel(x0 + x, y0 + y, color);
        ST7735_DrawPixel(x0 - x, y0 + y, color);
        ST7735_DrawPixel(x0 + x, y0 - y, color);
        ST7735_DrawPixel(x0 - x, y0 - y, color);
        ST7735_DrawPixel(x0 + y, y0 + x, color);
        ST7735_DrawPixel(x0 - y, y0 + x, color);
        ST7735_DrawPixel(x0 + y, y0 - x, color);
        ST7735_DrawPixel(x0 - y, y0 - x, color);
    }
}

// 绘制字符
void ST7735_DrawChar(uint8_t x, uint8_t y, char ch, uint16_t color, uint16_t bg) {
    if((ch < 32) || (ch > 126)) return; // 只支持可打印字符
    
    uint8_t index = ch - 32;
    
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t line = font5x7[index][i];
        for(uint8_t j = 0; j < 7; j++) {
            if(line & 0x01) {
                ST7735_DrawPixel(x + i, y + j, color);
            } else if(bg != color) {
                ST7735_DrawPixel(x + i, y + j, bg);
            }
            line >>= 1;
        }
    }
    
    // 字符间的空白列
    for(uint8_t j = 0; j < 7; j++) {
        if(bg != color) {
            ST7735_DrawPixel(x + 5, y + j, bg);
        }
    }
}

// 绘制字符串
void ST7735_DrawString(uint8_t x, uint8_t y, const char* str, uint16_t color, uint16_t bg) {
    uint8_t x_pos = x;
    while(*str) {
        ST7735_DrawChar(x_pos, y, *str, color, bg);
        x_pos += 6; // 字符宽度+间距
        str++;
    }
}

// 控制背光
void ST7735_Backlight(uint8_t state) {
    bcm2835_gpio_write(BL_PIN, state ? HIGH : LOW);
}

// 清理资源
void ST7735_Cleanup(void) {
    ST7735_Backlight(0);
    bcm2835_spi_end();
    bcm2835_close();
}

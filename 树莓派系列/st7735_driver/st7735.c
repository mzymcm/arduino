#include "st7735.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <math.h>

// GPIO引脚定义 (BCM编号)
#define ST7735_RST_PIN   27
#define ST7735_DC_PIN    25
#define ST7735_CS_PIN    8
#define ST7735_BL_PIN    24

// ST7735命令定义
#define ST7735_NOP       0x00
#define ST7735_SWRESET   0x01
#define ST7735_RDDID     0x04
#define ST7735_RDDST     0x09
#define ST7735_SLPIN     0x10
#define ST7735_SLPOUT    0x11
#define ST7735_PTLON     0x12
#define ST7735_NORON     0x13
#define ST7735_INVOFF    0x20
#define ST7735_INVON     0x21
#define ST7735_DISPOFF   0x28
#define ST7735_DISPON    0x29
#define ST7735_CASET     0x2A
#define ST7735_RASET     0x2B
#define ST7735_RAMWR     0x2C
#define ST7735_RAMRD     0x2E
#define ST7735_COLMOD    0x3A
#define ST7735_MADCTL    0x36

// MADCTL参数
#define MADCTL_MY        0x80
#define MADCTL_MX        0x40
#define MADCTL_MV        0x20
#define MADCTL_ML        0x10
#define MADCTL_RGB       0x00
#define MADCTL_BGR       0x08
#define MADCTL_MH        0x04

// SPI设备
#define SPI_DEVICE "/dev/spidev0.0"
static int spi_fd = -1;

// GPIO控制函数
static void gpio_export(int pin) {
    char buffer[64];
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd >= 0) {
        snprintf(buffer, sizeof(buffer), "%d", pin);
        write(fd, buffer, strlen(buffer));
        close(fd);
        usleep(100000);
    }
}

static void gpio_unexport(int pin) {
    char buffer[64];
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd >= 0) {
        snprintf(buffer, sizeof(buffer), "%d", pin);
        write(fd, buffer, strlen(buffer));
        close(fd);
    }
}

static void gpio_set_direction(int pin, const char *direction) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, direction, strlen(direction));
        close(fd);
    }
}

static void gpio_set_value(int pin, int value) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, value ? "1" : "0", 1);
        close(fd);
    }
}

// SPI传输函数
static void spi_transfer(const uint8_t *tx, uint8_t *rx, int length) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = length,
        .delay_usecs = 0,
        .speed_hz = 16000000,
        .bits_per_word = 8,
    };
    
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

// ST7735命令发送函数
static void st7735_write_command(uint8_t cmd) {
    gpio_set_value(ST7735_DC_PIN, 0);
    gpio_set_value(ST7735_CS_PIN, 0);
    spi_transfer(&cmd, NULL, 1);
    gpio_set_value(ST7735_CS_PIN, 1);
}

static void st7735_write_data(uint8_t data) {
    gpio_set_value(ST7735_DC_PIN, 1);
    gpio_set_value(ST7735_CS_PIN, 0);
    spi_transfer(&data, NULL, 1);
    gpio_set_value(ST7735_CS_PIN, 1);
}

static void st7735_write_data_bulk(const uint8_t *data, int length) {
    gpio_set_value(ST7735_DC_PIN, 1);
    gpio_set_value(ST7735_CS_PIN, 0);
    spi_transfer(data, NULL, length);
    gpio_set_value(ST7735_CS_PIN, 1);
}

// 设置显示窗口
static void st7735_set_window(st7735_t *dev, uint16_t x0, uint16_t y0, 
                             uint16_t x1, uint16_t y1) {
    gpio_set_value(ST7735_CS_PIN, 0);
    
    st7735_write_command(ST7735_CASET);
    st7735_write_data(x0 >> 8);
    st7735_write_data(x0 & 0xFF);
    st7735_write_data(x1 >> 8);
    st7735_write_data(x1 & 0xFF);
    
    st7735_write_command(ST7735_RASET);
    st7735_write_data(y0 >> 8);
    st7735_write_data(y0 & 0xFF);
    st7735_write_data(y1 >> 8);
    st7735_write_data(y1 & 0xFF);
    
    st7735_write_command(ST7735_RAMWR);
    
    gpio_set_value(ST7735_CS_PIN, 1);
}

// 初始化函数
int st7735_init(st7735_t *dev, st7735_rotation_t rotation) {
    if (!dev) return -1;
    
    // 设置设备参数
    dev->rotation = rotation;
    
    // 根据旋转方向设置宽高
    if (rotation == ST7735_ROTATION_0 || rotation == ST7735_ROTATION_180) {
        dev->width = 128;
        dev->height = 160;
    } else {
        dev->width = 160;
        dev->height = 128;
    }
    
    // 分配framebuffer内存
    dev->framebuffer = (uint16_t *)malloc(dev->width * dev->height * sizeof(uint16_t));
    if (!dev->framebuffer) {
        fprintf(stderr, "Error: Failed to allocate framebuffer\n");
        return -1;
    }
    
    // 初始化GPIO
    gpio_export(ST7735_RST_PIN);
    gpio_export(ST7735_DC_PIN);
    gpio_export(ST7735_CS_PIN);
    gpio_export(ST7735_BL_PIN);
    
    gpio_set_direction(ST7735_RST_PIN, "out");
    gpio_set_direction(ST7735_DC_PIN, "out");
    gpio_set_direction(ST7735_CS_PIN, "out");
    gpio_set_direction(ST7735_BL_PIN, "out");
    
    // 默认状态
    gpio_set_value(ST7735_CS_PIN, 1);
    gpio_set_value(ST7735_DC_PIN, 1);
    gpio_set_value(ST7735_RST_PIN, 1);
    gpio_set_value(ST7735_BL_PIN, 0);
    
    // 初始化SPI
    spi_fd = open(SPI_DEVICE, O_RDWR);
    if (spi_fd < 0) {
        fprintf(stderr, "Error: Failed to open SPI device\n");
        free(dev->framebuffer);
        return -1;
    }
    
    // 设置SPI模式
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 16000000;
    
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_RD_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    
    // 硬件复位
    gpio_set_value(ST7735_RST_PIN, 0);
    usleep(100000);
    gpio_set_value(ST7735_RST_PIN, 1);
    usleep(100000);
    
    // 开始初始化序列
    gpio_set_value(ST7735_CS_PIN, 0);
    
    // 软件复位
    st7735_write_command(ST7735_SWRESET);
    usleep(150000);
    
    // 退出睡眠模式
    st7735_write_command(ST7735_SLPOUT);
    usleep(150000);
    
    // 帧率控制
    st7735_write_command(0xB1);
    st7735_write_data(0x01);
    st7735_write_data(0x2C);
    st7735_write_data(0x2D);
    
    st7735_write_command(0xB2);
    st7735_write_data(0x01);
    st7735_write_data(0x2C);
    st7735_write_data(0x2D);
    
    st7735_write_command(0xB3);
    st7735_write_data(0x01);
    st7735_write_data(0x2C);
    st7735_write_data(0x2D);
    st7735_write_data(0x01);
    st7735_write_data(0x2C);
    st7735_write_data(0x2D);
    
    // 显示反转
    st7735_write_command(0xB4);
    st7735_write_data(0x07);
    
    // 电源控制
    st7735_write_command(0xC0);
    st7735_write_data(0xA2);
    st7735_write_data(0x02);
    st7735_write_data(0x84);
    
    st7735_write_command(0xC1);
    st7735_write_data(0xC5);
    
    st7735_write_command(0xC2);
    st7735_write_data(0x0A);
    st7735_write_data(0x00);
    
    st7735_write_command(0xC3);
    st7735_write_data(0x8A);
    st7735_write_data(0x2A);
    
    st7735_write_command(0xC4);
    st7735_write_data(0x8A);
    st7735_write_data(0xEE);
    
    st7735_write_command(0xC5);
    st7735_write_data(0x0E);
    
    // Gamma校正
    st7735_write_command(0xE0);
    uint8_t gamma_pos[] = {
        0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28, 0x20, 0x22,
        0x1F, 0x1B, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10
    };
    st7735_write_data_bulk(gamma_pos, sizeof(gamma_pos));
    
    st7735_write_command(0xE1);
    uint8_t gamma_neg[] = {
        0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C, 0x29, 0x2E,
        0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10
    };
    st7735_write_data_bulk(gamma_neg, sizeof(gamma_neg));
    
    // 颜色模式：16位RGB565
    st7735_write_command(ST7735_COLMOD);
    st7735_write_data(0x05);
    
    // 设置显示方向
    st7735_set_rotation(dev, rotation);
    
    // 正常显示模式
    st7735_write_command(ST7735_NORON);
    usleep(10000);
    
    // 开启显示
    st7735_write_command(ST7735_DISPON);
    usleep(100000);
    
    gpio_set_value(ST7735_CS_PIN, 1);
    
    // 开启背光
    st7735_set_backlight(true);
    
    // 清屏
    st7735_clear(dev, ST7735_BLACK);
    
    return 0;
}

void st7735_deinit(st7735_t *dev) {
    if (!dev) return;
    
    // 关闭背光
    st7735_set_backlight(false);
    
    // 清屏
    st7735_clear(dev, ST7735_BLACK);
    st7735_update(dev);
    
    // 关闭显示
    gpio_set_value(ST7735_CS_PIN, 0);
    st7735_write_command(ST7735_DISPOFF);
    st7735_write_command(ST7735_SLPIN);
    gpio_set_value(ST7735_CS_PIN, 1);
    
    // 释放资源
    if (dev->framebuffer) {
        free(dev->framebuffer);
        dev->framebuffer = NULL;
    }
    
    if (spi_fd >= 0) {
        close(spi_fd);
        spi_fd = -1;
    }
    
    // 取消GPIO导出
    gpio_unexport(ST7735_RST_PIN);
    gpio_unexport(ST7735_DC_PIN);
    gpio_unexport(ST7735_CS_PIN);
    gpio_unexport(ST7735_BL_PIN);
}

// 设置显示方向
void st7735_set_rotation(st7735_t *dev, st7735_rotation_t rotation) {
    if (!dev) return;
    
    dev->rotation = rotation;
    
    uint8_t madctl = 0;
    
    switch (rotation) {
        case ST7735_ROTATION_0:
            madctl = MADCTL_MX | MADCTL_MY | MADCTL_BGR;
            dev->width = 128;
            dev->height = 160;
            break;
        case ST7735_ROTATION_90:
            madctl = MADCTL_MY | MADCTL_MV | MADCTL_BGR;
            dev->width = 160;
            dev->height = 128;
            break;
        case ST7735_ROTATION_180:
            madctl = MADCTL_BGR;
            dev->width = 128;
            dev->height = 160;
            break;
        case ST7735_ROTATION_270:
            madctl = MADCTL_MX | MADCTL_MV | MADCTL_BGR;
            dev->width = 160;
            dev->height = 128;
            break;
    }
    
    gpio_set_value(ST7735_CS_PIN, 0);
    st7735_write_command(ST7735_MADCTL);
    st7735_write_data(madctl);
    gpio_set_value(ST7735_CS_PIN, 1);
}

// 清屏
void st7735_clear(st7735_t *dev, uint16_t color) {
    if (!dev || !dev->framebuffer) return;
    
    for (int i = 0; i < dev->width * dev->height; i++) {
        dev->framebuffer[i] = color;
    }
}

// 设置像素
void st7735_set_pixel(st7735_t *dev, uint16_t x, uint16_t y, uint16_t color) {
    if (!dev || !dev->framebuffer) return;
    if (x >= dev->width || y >= dev->height) return;
    
    dev->framebuffer[y * dev->width + x] = color;
}

// 绘制矩形框
void st7735_draw_rect(st7735_t *dev, uint16_t x, uint16_t y, 
                     uint16_t w, uint16_t h, uint16_t color) {
    if (!dev) return;
    
    // 上边
    for (uint16_t i = x; i < x + w && i < dev->width; i++) {
        st7735_set_pixel(dev, i, y, color);
    }
    
    // 下边
    if (y + h - 1 < dev->height) {
        for (uint16_t i = x; i < x + w && i < dev->width; i++) {
            st7735_set_pixel(dev, i, y + h - 1, color);
        }
    }
    
    // 左边
    for (uint16_t i = y; i < y + h && i < dev->height; i++) {
        st7735_set_pixel(dev, x, i, color);
    }
    
    // 右边
    if (x + w - 1 < dev->width) {
        for (uint16_t i = y; i < y + h && i < dev->height; i++) {
            st7735_set_pixel(dev, x + w - 1, i, color);
        }
    }
}

// 填充矩形
void st7735_fill_rect(st7735_t *dev, uint16_t x, uint16_t y,
                     uint16_t w, uint16_t h, uint16_t color) {
    if (!dev) return;
    
    for (uint16_t j = y; j < y + h && j < dev->height; j++) {
        for (uint16_t i = x; i < x + w && i < dev->width; i++) {
            st7735_set_pixel(dev, i, j, color);
        }
    }
}

// 绘制直线（Bresenham算法）
void st7735_draw_line(st7735_t *dev, uint16_t x0, uint16_t y0,
                     uint16_t x1, uint16_t y1, uint16_t color) {
    if (!dev) return;
    
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        st7735_set_pixel(dev, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// 绘制圆形框
void st7735_draw_circle(st7735_t *dev, uint16_t x0, uint16_t y0,
                       uint16_t r, uint16_t color) {
    if (!dev) return;
    
    int x = r;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        st7735_set_pixel(dev, x0 + x, y0 + y, color);
        st7735_set_pixel(dev, x0 + y, y0 + x, color);
        st7735_set_pixel(dev, x0 - y, y0 + x, color);
        st7735_set_pixel(dev, x0 - x, y0 + y, color);
        st7735_set_pixel(dev, x0 - x, y0 - y, color);
        st7735_set_pixel(dev, x0 - y, y0 - x, color);
        st7735_set_pixel(dev, x0 + y, y0 - x, color);
        st7735_set_pixel(dev, x0 + x, y0 - y, color);
        
        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

// 填充圆形
void st7735_fill_circle(st7735_t *dev, uint16_t x0, uint16_t y0,
                       uint16_t r, uint16_t color) {
    if (!dev) return;
    
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                st7735_set_pixel(dev, x0 + x, y0 + y, color);
            }
        }
    }
}

// 更新显示
void st7735_update(st7735_t *dev) {
    if (!dev || !dev->framebuffer) return;
    
    // 设置全屏窗口
    st7735_set_window(dev, 0, 0, dev->width - 1, dev->height - 1);
    
    gpio_set_value(ST7735_DC_PIN, 1);
    gpio_set_value(ST7735_CS_PIN, 0);
    
    // 批量发送framebuffer数据
    spi_transfer((const uint8_t *)dev->framebuffer, NULL, 
                 dev->width * dev->height * 2);
    
    gpio_set_value(ST7735_CS_PIN, 1);
}

// 控制背光
void st7735_set_backlight(bool state) {
    gpio_set_value(ST7735_BL_PIN, state ? 1 : 0);
}

// RGB888转RGB565
uint16_t st7735_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// 8x8字体数据
static const uint8_t font_8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 空格
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00}, // "
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // #
    {0x30,0x7C,0xC0,0x78,0x0C,0xF8,0x30,0x00}, // $
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, // %
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // &
    {0x60,0x60,0xC0,0x00,0x00,0x00,0x00,0x00}, // '
    {0x18,0x30,0x60,0x60,0x60,0x30,0x18,0x00}, // (
    {0x60,0x30,0x18,0x18,0x18,0x30,0x60,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x30,0x30,0xFC,0x30,0x30,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x60}, // ,
    {0x00,0x00,0x00,0xFC,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00}, // .
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // /
    {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00}, // 0
    {0x30,0x70,0x30,0x30,0x30,0x30,0xFC,0x00}, // 1
    {0x78,0xCC,0x0C,0x38,0x60,0xCC,0xFC,0x00}, // 2
    {0x78,0xCC,0x0C,0x38,0x0C,0xCC,0x78,0x00}, // 3
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, // 4
    {0xFC,0xC0,0xF8,0x0C,0x0C,0xCC,0x78,0x00}, // 5
    {0x38,0x60,0xC0,0xF8,0xCC,0xCC,0x78,0x00}, // 6
    {0xFC,0xCC,0x0C,0x18,0x30,0x30,0x30,0x00}, // 7
    {0x78,0xCC,0xCC,0x78,0xCC,0xCC,0x78,0x00}, // 8
    {0x78,0xCC,0xCC,0x7C,0x0C,0x18,0x70,0x00}, // 9
    {0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x00}, // :
    {0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x60}, // ;
    {0x18,0x30,0x60,0xC0,0x60,0x30,0x18,0x00}, // <
    {0x00,0x00,0xFC,0x00,0x00,0xFC,0x00,0x00}, // =
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, // >
    {0x78,0xCC,0x0C,0x18,0x30,0x00,0x30,0x00}, // ?
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00}, // @
    {0x30,0x78,0xCC,0xCC,0xFC,0xCC,0xCC,0x00}, // A
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, // B
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, // C
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, // D
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, // E
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, // F
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, // G
    {0xCC,0xCC,0xCC,0xFC,0xCC,0xCC,0xCC,0x00}, // H
    {0x78,0x30,0x30,0x30,0x30,0x30,0x78,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, // J
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, // K
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, // L
    {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00}, // M
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // N
    {0x38,0x6C,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // O
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, // P
    {0x78,0xCC,0xCC,0xCC,0xDC,0x78,0x1C,0x00}, // Q
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, // R
    {0x78,0xCC,0xE0,0x70,0x1C,0xCC,0x78,0x00}, // S
    {0xFC,0xB4,0x30,0x30,0x30,0x30,0x78,0x00}, // T
    {0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xFC,0x00}, // U
    {0xCC,0xCC,0xCC,0xCC,0xCC,0x78,0x30,0x00}, // V
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, // W
    {0xC6,0xC6,0x6C,0x38,0x38,0x6C,0xC6,0x00}, // X
    {0xCC,0xCC,0xCC,0x78,0x30,0x30,0x78,0x00}, // Y
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, // Z
    {0x78,0x60,0x60,0x60,0x60,0x60,0x78,0x00}, // [
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // backslash
    {0x78,0x18,0x18,0x18,0x18,0x18,0x78,0x00}, // ]
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, // a
    {0xE0,0x60,0x60,0x7C,0x66,0x66,0xDC,0x00}, // b
    {0x00,0x00,0x78,0xCC,0xC0,0xCC,0x78,0x00}, // c
    {0x1C,0x0C,0x0C,0x7C,0xCC,0xCC,0x76,0x00}, // d
    {0x00,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00}, // e
    {0x38,0x6C,0x60,0xF0,0x60,0x60,0xF0,0x00}, // f
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, // g
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, // h
    {0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00}, // i
    {0x0C,0x00,0x0C,0x0C,0x0C,0xCC,0xCC,0x78}, // j
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, // k
    {0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00}, // l
    {0x00,0x00,0xCC,0xFE,0xFE,0xD6,0xC6,0x00}, // m
    {0x00,0x00,0xF8,0xCC,0xCC,0xCC,0xCC,0x00}, // n
    {0x00,0x00,0x78,0xCC,0xCC,0xCC,0x78,0x00}, // o
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, // p
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, // q
    {0x00,0x00,0xDC,0x76,0x66,0x60,0xF0,0x00}, // r
    {0x00,0x00,0x7C,0xC0,0x78,0x0C,0xF8,0x00}, // s
    {0x10,0x30,0x7C,0x30,0x30,0x34,0x18,0x00}, // t
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, // u
    {0x00,0x00,0xCC,0xCC,0xCC,0x78,0x30,0x00}, // v
    {0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x6C,0x00}, // w
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // x
    {0x00,0x00,0xCC,0xCC,0xCC,0x7C,0x0C,0xF8}, // y
    {0x00,0x00,0xFC,0x98,0x30,0x64,0xFC,0x00}, // z
    {0x1C,0x30,0x30,0xE0,0x30,0x30,0x1C,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0xE0,0x30,0x30,0x1C,0x30,0x30,0xE0,0x00}, // }
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
};

// 绘制字符
void st7735_draw_char(st7735_t *dev, char ch, uint16_t x, uint16_t y,
                     uint16_t color, uint16_t bg_color, uint8_t size) {
    if (!dev || ch < 32 || ch > 126) return;
    
    const uint8_t *char_data = font_8x8[ch - 32];
    
    for (uint8_t j = 0; j < 8; j++) {
        uint8_t line = char_data[j];
        for (uint8_t i = 0; i < 8; i++) {
            if (line & 0x80) {
                if (size == 1) {
                    st7735_set_pixel(dev, x + i, y + j, color);
                } else {
                    st7735_fill_rect(dev, x + i * size, y + j * size,
                                    size, size, color);
                }
            } else if (bg_color != color) {
                if (size == 1) {
                    st7735_set_pixel(dev, x + i, y + j, bg_color);
                } else {
                    st7735_fill_rect(dev, x + i * size, y + j * size,
                                    size, size, bg_color);
                }
            }
            line <<= 1;
        }
    }
}

// 绘制字符串
void st7735_draw_string(st7735_t *dev, const char *str, uint16_t x, uint16_t y,
                       uint16_t color, uint16_t bg_color, uint8_t size) {
    if (!dev || !str) return;
    
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    
    while (*str) {
        if (*str == '\n') {
            cursor_x = x;
            cursor_y += 8 * size;
        } else {
            st7735_draw_char(dev, *str, cursor_x, cursor_y, color, bg_color, size);
            cursor_x += 8 * size;
        }
        str++;
    }
}
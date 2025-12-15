#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bcm2835.h>
#include "st7735.h"

int main() {
    // 初始化BCM2835库
    if(!bcm2835_init()) {
        printf("BCM2835初始化失败!\n");
        return 1;
    }
    
    printf("初始化ST7735显示屏...\n");
    ST7735_Init();
    
    // 测试1: 填充不同颜色
    printf("测试1: 颜色填充...\n");
    ST7735_FillScreen(RED);
    sleep(1);
    ST7735_FillScreen(GREEN);
    sleep(1);
    ST7735_FillScreen(BLUE);
    sleep(1);
    ST7735_FillScreen(BLACK);
    
    // 测试2: 绘制图形
    printf("测试2: 绘制图形...\n");
    ST7735_FillScreen(BLACK);
    
    // 绘制矩形
    ST7735_DrawRectangle(10, 10, 50, 30, RED);
    ST7735_FillRectangle(70, 10, 40, 30, BLUE);
    
    // 绘制直线
    ST7735_DrawLine(10, 50, 120, 50, GREEN);
    ST7735_DrawLine(10, 50, 65, 120, YELLOW);
    ST7735_DrawLine(120, 50, 65, 120, CYAN);
    
    // 绘制圆形
    ST7735_DrawCircle(95, 85, 20, MAGENTA);
    
    sleep(2);
    
    // 测试3: 显示文字
    printf("测试3: 显示文字...\n");
    ST7735_FillScreen(BLACK);
    
    ST7735_DrawString(10, 10, "Raspberry Pi A1", GREEN, BLACK);
    ST7735_DrawString(10, 30, "ST7735 128x128", YELLOW, BLACK);
    ST7735_DrawString(10, 50, "Display Test", CYAN, BLACK);
    ST7735_DrawString(10, 70, "Hello World!", MAGENTA, BLACK);
    
    sleep(3);
    
    // 测试4: 渐变效果
    printf("测试4: 渐变效果...\n");
    for(int i = 0; i < 128; i++) {
        for(int j = 0; j < 128; j++) {
            uint16_t color = ((i * 31 / 127) << 11) | ((j * 63 / 127) << 5);
            ST7735_DrawPixel(i, j, color);
        }
    }
    
    sleep(2);
    
    printf("测试完成!\n");
    ST7735_Cleanup();
    
    return 0;
}

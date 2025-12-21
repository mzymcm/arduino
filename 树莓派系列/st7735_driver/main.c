#include "st7735.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

// 测试图案
void test_pattern(st7735_t *lcd) {
    printf("显示测试图案...\n");
    
    // 清屏为黑色
    st7735_clear(lcd, ST7735_BLACK);
    st7735_update(lcd);
    sleep(1);
    
    // 颜色条
    printf("显示颜色条...\n");
    int bar_width = lcd->width / 7;
    st7735_fill_rect(lcd, 0, 0, bar_width, lcd->height, ST7735_RED);
    st7735_fill_rect(lcd, bar_width, 0, bar_width, lcd->height, ST7735_GREEN);
    st7735_fill_rect(lcd, bar_width*2, 0, bar_width, lcd->height, ST7735_BLUE);
    st7735_fill_rect(lcd, bar_width*3, 0, bar_width, lcd->height, ST7735_YELLOW);
    st7735_fill_rect(lcd, bar_width*4, 0, bar_width, lcd->height, ST7735_MAGENTA);
    st7735_fill_rect(lcd, bar_width*5, 0, bar_width, lcd->height, ST7735_CYAN);
    st7735_fill_rect(lcd, bar_width*6, 0, lcd->width-bar_width*6, lcd->height, ST7735_WHITE);
    
    st7735_update(lcd);
    sleep(2);
    
    // 几何图形
    printf("绘制几何图形...\n");
    st7735_clear(lcd, ST7735_BLACK);
    
    // 矩形
    st7735_draw_rect(lcd, 10, 10, 50, 30, ST7735_RED);
    st7735_fill_rect(lcd, 70, 10, 50, 30, ST7735_GREEN);
    
    // 圆形
    st7735_draw_circle(lcd, 40, 70, 20, ST7735_BLUE);
    st7735_fill_circle(lcd, 100, 70, 20, ST7735_YELLOW);
    
    // 直线
    for (int i = 0; i < 5; i++) {
        st7735_draw_line(lcd, 10, 110 + i, 150, 120 + i, ST7735_CYAN);
    }
    
    st7735_update(lcd);
    sleep(2);
    
    // 显示文本
    printf("显示文本...\n");
    st7735_clear(lcd, ST7735_BLACK);
    
    st7735_draw_string(lcd, "Raspberry Pi", 20, 10, 
                      ST7735_WHITE, ST7735_BLACK, 2);
    st7735_draw_string(lcd, "ST7735 Driver", 10, 30,
                      ST7735_GREEN, ST7735_BLACK, 2);
    st7735_draw_string(lcd, "160x128 LCD", 20, 50,
                      ST7735_RED, ST7735_BLACK, 2);
    st7735_draw_string(lcd, "SPI Interface", 10, 70,
                      ST7735_BLUE, ST7735_BLACK, 1);
    
    st7735_update(lcd);
    sleep(3);
}

// 动画演示
void animation_demo(st7735_t *lcd) {
    printf("动画演示...\n");
    
    int center_x = lcd->width / 2;
    int center_y = lcd->height / 2;
    float angle = 0;
    
    for (int frame = 0; frame < 50; frame++) {
        st7735_clear(lcd, ST7735_BLACK);
        
        // 旋转的方块
        for (int i = 0; i < 4; i++) {
            float a = angle + i * M_PI_2;
            int x = center_x + (int)(40 * cos(a));
            int y = center_y + (int)(40 * sin(a));
            
            // 使用HSV颜色生成彩虹效果
            uint16_t color = st7735_color_rgb(
                (int)(127.5 * (1 + cos(a))),
                (int)(127.5 * (1 + sin(a))),
                (int)(127.5 * (1 + cos(a + M_PI_2)))
            );
            
            st7735_fill_rect(lcd, x - 8, y - 8, 16, 16, color);
        }
        
        // 进度条
        int progress = (frame * lcd->width) / 50;
        st7735_fill_rect(lcd, 0, lcd->height - 10, progress, 8, 
                       st7735_color_rgb(progress * 255 / lcd->width, 100, 255));
        
        // 显示帧数
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "Frame: %d", frame);
        st7735_draw_string(lcd, buffer, 5, 5, ST7735_WHITE, ST7735_BLACK, 1);
        
        st7735_update(lcd);
        
        angle += 0.1;
        usleep(50000);
    }
}

// 渐变效果
void gradient_demo(st7735_t *lcd) {
    printf("渐变效果...\n");
    
    // 水平渐变
    for (int x = 0; x < lcd->width; x++) {
        uint8_t r = x * 255 / lcd->width;
        uint8_t g = 128;
        uint8_t b = 255 - x * 255 / lcd->width;
        
        uint16_t color = st7735_color_rgb(r, g, b);
        st7735_draw_line(lcd, x, 0, x, lcd->height - 1, color);
    }
    
    st7735_update(lcd);
    sleep(2);
    
    // 垂直渐变
    st7735_clear(lcd, ST7735_BLACK);
    
    for (int y = 0; y < lcd->height; y++) {
        uint8_t r = y * 255 / lcd->height;
        uint8_t g = 255 - y * 255 / lcd->height;
        uint8_t b = 128;
        
        uint16_t color = st7735_color_rgb(r, g, b);
        st7735_draw_line(lcd, 0, y, lcd->width - 1, y, color);
    }
    
    st7735_update(lcd);
    sleep(2);
}

int main() {
    printf("ST7735 LCD 驱动测试\n");
    printf("引脚配置:\n");
    printf("  SCLK -> GPIO11 (引脚23)\n");
    printf("  MOSI -> GPIO10 (引脚19)\n");
    printf("  DC   -> GPIO25 (引脚22)\n");
    printf("  CS   -> GPIO8  (引脚24)\n");
    printf("  RST  -> GPIO27 (引脚13)\n");
    printf("  BL   -> GPIO24 (引脚18)\n");
    
    // 初始化LCD
    st7735_t lcd;
    int ret = st7735_init(&lcd, ST7735_ROTATION_90);
    
    if (ret < 0) {
        fprintf(stderr, "初始化失败\n");
        return 1;
    }
    
    printf("\n初始化成功\n");
    printf("分辨率: %dx%d\n", lcd.width, lcd.height);
    
    // 运行测试
    test_pattern(&lcd);
    animation_demo(&lcd);
    gradient_demo(&lcd);
    
    // 最终显示
    printf("\n最终显示...\n");
    st7735_clear(&lcd, ST7735_BLACK);
    
    // 显示系统信息
    st7735_draw_string(&lcd, "ST7735 Demo", 30, 10, ST7735_WHITE, ST7735_BLACK, 2);
    st7735_draw_string(&lcd, "Complete!", 40, 40, ST7735_GREEN, ST7735_BLACK, 2);
    st7735_draw_string(&lcd, "Press Ctrl+C", 20, 70, ST7735_YELLOW, ST7735_BLACK, 1);
    st7735_draw_string(&lcd, "to exit", 50, 85, ST7735_YELLOW, ST7735_BLACK, 1);
    
    st7735_update(&lcd);
    
    printf("测试完成，按Ctrl+C退出...\n");
    
    // 保持显示
    while (1) {
        sleep(1);
    }
    
    // 清理
    st7735_deinit(&lcd);
    printf("资源已清理\n");
    
    return 0;
}
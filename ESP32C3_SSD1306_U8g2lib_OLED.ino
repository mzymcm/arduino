#include <U8g2lib.h>
#include <SPI.h>

// 引脚定义
#define OLED_CS   7
#define OLED_DC   6
#define OLED_MOSI 3  // SDA
#define OLED_SCLK 2  // SCL

// 根据你的引脚配置 (4线软件SPI)
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(
  U8G2_R0, 
  /* clock=*/ OLED_SCLK,  // GPIO2
  /* data=*/  OLED_MOSI,  // GPIO3
  /* cs=*/    OLED_CS,    // GPIO7
  /* dc=*/    OLED_DC,    // GPIO6
  /* reset=*/ U8X8_PIN_NONE
);

void setup(void) {
  u8g2.begin();           // 初始化U8g2
  u8g2.enableUTF8Print(); // 启用UTF-8支持，对显示中文很重要[citation:2]
}

void loop(void) {
  u8g2.clearBuffer();         // 清除内部缓冲区
  
  // 设置一个包含中文的字体[citation:2]
  u8g2.setFont(u8g2_font_unifont_t_chinese2);  // 这个字体包含一些常用汉字
  
  u8g2.setCursor(0, 20); 
  u8g2.print("Hello World!");  // 测试英文显示

  u8g2.setCursor(0, 40); 
  u8g2.print("你好世界");      // 尝试显示中文

  u8g2.sendBuffer();          // 将缓冲区内容发送到显示器
  delay(5000);                // 延迟5秒以便观察
}
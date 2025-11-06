#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// 合宙Air001迷你板 SPI 引脚定义
#define OLED_CS   PA4
#define OLED_DC   PA6
#define OLED_MOSI PA7  // SDA
#define OLED_SCLK PA5  // SCL
#define OLED_RST  -1  // 复位引脚，如果未连接则设为-1

// OLED显示参数
#define SCREEN_WIDTH 128  // OLED显示宽度，像素
#define SCREEN_HEIGHT 64  // OLED显示高度，像素

// 创建SSD1306显示对象（软件SPI）
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_SCLK, OLED_DC, OLED_RST, OLED_CS);

void setup() {
  Serial.begin(115200);
  Serial.println("Luat Air001 SSD1306 OLED Test Start");
  
  // 初始化OLED显示
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // 停止执行
  }
  Serial.println("SSD1306 Init Success");
  
  // 显示初始画面
  display.clearDisplay();
  display.display();
  delay(1000);
  
  // 显示欢迎信息（使用英文）
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Luat Air001");
  display.setTextSize(1);
  display.println("OLED Test");
  display.println("SSD1306");
  display.println("Display OK!");
  display.display();
  delay(3000);
}

void loop() {
  // 测试1：显示文本（英文）
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println("Text Test:");
  display.println("Hello World!");
  display.println("ABCDEFG 12345");
  display.println("!@#$%^&*()");
  display.println("Good Display!");
  display.display();
  delay(3000);
   
  Serial.println("Test cycle completed, restarting...");
}
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>


//==ST7735信号线	 (NANO默认SPI)
//==LED (背光控制)	7
//==SCK (SPI时钟)	13
//==SDA (MOSI)	11
//==A0 (DC)		8
//==RST (复位信号)	9
//==CS (片选信号)	10

// Arduino Nano引脚定义
#define TFT_CS   10  // Nano的常用片选引脚
#define TFT_DC   8   // 数据/命令选择引脚
#define TFT_RST  9   // 复位引脚
#define TFT_BL   7   // 背光控制引脚

// 使用硬件SPI (Nano的默认SPI引脚: 11-MOSI, 13-SCK)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  // 初始化串口
  Serial.begin(9600);
  Serial.println("ST7735 Test!");
  
  // 初始化背光
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // 初始化显示屏
  tft.initR(INITR_144GREENTAB); // 针对128x128屏幕
  
  // 设置屏幕方向
  tft.setRotation(2); // 0-3，根据实际显示方向调整
  
  // 填充黑色背景
  tft.fillScreen(ST77XX_BLACK);
  
  // 显示测试内容
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Nano + ST7735");
  
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println("128x128 Display");
  
  // 绘制一些图形
  tft.drawRect(10, 60, 50, 30, ST77XX_RED);    // 红色矩形
  tft.fillCircle(90, 75, 15, ST77XX_BLUE);     // 蓝色填充圆
  tft.drawLine(10, 100, 118, 100, ST77XX_GREEN); // 绿色直线
}

void loop() {
  // 添加动态效果
  static int counter = 0;
  
  tft.fillRect(10, 110, 108, 15, ST77XX_BLACK); // 清空计数区域
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 110);
  tft.print("Counter: ");
  tft.print(counter);
  
  counter++;
  delay(1000);
}
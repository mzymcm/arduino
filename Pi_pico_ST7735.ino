#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

//==ST7735信号线		(Pico默认SPI)
//==LED (背光控制)		GP21
//==SCK (SPI时钟)		GP18
//==SDA (MOSI)		GP19
//==A0 (DC)		GP22
//==RST (复位信号)		GP20
//==CS (片选信号)		GP17

// Raspberry Pi Pico 默认SPI引脚定义
// 使用SPI0接口，这是Pico的默认SPI
#define TFT_BL   21		// LED (背光控制)
#define TFT_SCLK 18		// Clock out SCK (SPI时钟)
#define TFT_MOSI 19	// Data out SDA (MOSI)
#define TFT_DC   22		// 数据/命令选择引脚 A0 (DC)
#define TFT_RST  20		// 复位引脚  RST (复位信号) 
#define TFT_CS   17		// 片选引脚，CS (片选信号)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  // 初始化串口
  Serial.begin(9600);
  Serial.println("ST7735 Test with Pico!");
  
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
  tft.println("Pico + ST7735");
  
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
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ST7735信号线定义
#define TFT_BL   21    // LED (背光控制)
#define TFT_SCLK 18    // Clock out SCK (SPI时钟)
#define TFT_MOSI 19    // Data out SDA (MOSI)
#define TFT_DC   22    // 数据/命令选择引脚 A0 (DC)
#define TFT_RST  20    // 复位引脚  RST (复位信号) 
#define TFT_CS   17    // 片选引脚，CS (片选信号)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// 自定义灰色（RGB565格式）
#define GRAY_COLOR 0x7BEF  // 中等灰色

// 时间变量
int hours = 14;
int minutes = 30;
int seconds = 0;

// 天气变量
float temperature = 25.5;
int humidity = 65;
String weatherStatus = "Sunny";

// 太空人动画变量
int astronautX = 90;
int astronautY = 90;
int astronautFrame = 0;
bool astronautDirection = true; // true = 向右, false = 向左

void setup() {
  // 初始化串口
  Serial.begin(9600);
  Serial.println("ST7735 Smart Display with Pico!");
  
  // 初始化背光
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // 初始化显示屏
  tft.initR(INITR_144GREENTAB); // 针对128x128屏幕
  
  // 设置屏幕方向
  tft.setRotation(2); // 0-3，根据实际显示方向调整
  
  // 填充黑色背景（太空背景）
  drawSpaceBackground();
  
  // 绘制初始界面
  drawStaticElements();
  drawTime();
  drawWeather();
  drawAstronaut();
}

void loop() {
  // 更新时间（模拟）
  updateTime();
  
  // 更新天气数据（模拟）
  updateWeather();
  
  // 更新太空人动画
  updateAstronaut();
  
  // 重绘动态内容
  drawTime();
  drawWeather();
  drawAstronaut();
  
  delay(1000); // 每秒更新一次
}

void drawSpaceBackground() {
  // 绘制深蓝色太空背景
  tft.fillScreen(ST77XX_BLACK);
  
  // 绘制星星
  tft.setTextColor(ST77XX_WHITE);
  for(int i = 0; i < 30; i++) {
    int x = random(0, 128);
    int y = random(0, 128);
    tft.drawPixel(x, y, ST77XX_WHITE);
  }
  
  // 绘制地球（左下角）
  tft.fillCircle(20, 108, 15, ST77XX_BLUE);
  tft.fillCircle(25, 105, 8, ST77XX_GREEN);
}

void drawStaticElements() {
  // 绘制标题
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(35, 5);
  tft.println("SPACE STATION");
  
  // 绘制分隔线
  tft.drawLine(0, 18, 128, 18, ST77XX_WHITE);
  
  // 绘制时间标签
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 25);
  tft.println("TIME:");
  
  // 绘制天气标签
  tft.setCursor(10, 50);
  tft.println("WEATHER:");
  
  // 绘制温度图标
  tft.setCursor(75, 50);
  tft.print("T:");
  
  // 绘制湿度图标
  tft.setCursor(75, 65);
  tft.print("H:");
}

void drawTime() {
  // 清除时间显示区域
  tft.fillRect(45, 25, 70, 15, ST77XX_BLACK);
  
  // 绘制时间
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(45, 25);
  
  // 格式化时间显示
  if(hours < 10) tft.print("0");
  tft.print(hours);
  tft.print(":");
  if(minutes < 10) tft.print("0");
  tft.print(minutes);
  tft.print(":");
  if(seconds < 10) tft.print("0");
  tft.print(seconds);
}

void drawWeather() {
  // 清除天气显示区域
  tft.fillRect(45, 50, 70, 30, ST77XX_BLACK);
  
  // 绘制天气状态
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(45, 50);
  tft.println(weatherStatus);
  
  // 绘制温度
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(85, 50);
  tft.print(temperature, 1);
  tft.print("C");
  
  // 绘制湿度
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(85, 65);
  tft.print(humidity);
  tft.print("%");
}

void drawAstronaut() {
  // 清除太空人区域
  tft.fillRect(astronautX-15, astronautY-15, 30, 30, ST77XX_BLACK);
  
  // 根据帧数绘制不同姿态的太空人
  if(astronautFrame % 4 < 2) {
    // 姿态1：正常飞行
    drawAstronautBody(astronautX, astronautY, astronautDirection);
  } else {
    // 姿态2：挥手
    drawAstronautWaving(astronautX, astronautY, astronautDirection);
  }
}

void drawAstronautBody(int x, int y, bool direction) {
  // 绘制太空服（白色）
  tft.fillCircle(x, y, 6, ST77XX_WHITE);
  tft.fillRect(x-6, y, 12, 10, ST77XX_WHITE);
  
  // 绘制面罩（深蓝色）
  tft.fillCircle(x, y, 4, ST77XX_BLUE);
  
  // 绘制背包 - 使用自定义灰色
  tft.fillRect(x+3, y+2, 4, 8, GRAY_COLOR);
  
  // 绘制四肢
  if(direction) {
    // 向右飞行
    tft.drawLine(x-6, y+3, x-12, y, ST77XX_WHITE); // 左臂
    tft.drawLine(x+6, y+3, x+10, y+8, ST77XX_WHITE); // 右臂
    tft.drawLine(x-3, y+10, x-8, y+15, ST77XX_WHITE); // 左腿
    tft.drawLine(x+3, y+10, x+8, y+15, ST77XX_WHITE); // 右腿
  } else {
    // 向左飞行
    tft.drawLine(x-6, y+3, x-10, y+8, ST77XX_WHITE); // 左臂
    tft.drawLine(x+6, y+3, x+12, y, ST77XX_WHITE); // 右臂
    tft.drawLine(x-3, y+10, x-8, y+15, ST77XX_WHITE); // 左腿
    tft.drawLine(x+3, y+10, x+8, y+15, ST77XX_WHITE); // 右腿
  }
}

void drawAstronautWaving(int x, int y, bool direction) {
  // 绘制太空服（白色）
  tft.fillCircle(x, y, 6, ST77XX_WHITE);
  tft.fillRect(x-6, y, 12, 10, ST77XX_WHITE);
  
  // 绘制面罩（深蓝色）
  tft.fillCircle(x, y, 4, ST77XX_BLUE);
  
  // 绘制背包 - 使用自定义灰色
  tft.fillRect(x+3, y+2, 4, 8, GRAY_COLOR);
  
  // 绘制四肢（挥手姿态）
  if(direction) {
    // 向右飞行并挥手
    tft.drawLine(x-6, y+3, x-12, y+8, ST77XX_WHITE); // 左臂挥手
    tft.drawLine(x+6, y+3, x+10, y, ST77XX_WHITE); // 右臂
    tft.drawLine(x-3, y+10, x-8, y+15, ST77XX_WHITE); // 左腿
    tft.drawLine(x+3, y+10, x+8, y+15, ST77XX_WHITE); // 右腿
  } else {
    // 向左飞行并挥手
    tft.drawLine(x-6, y+3, x-10, y, ST77XX_WHITE); // 左臂
    tft.drawLine(x+6, y+3, x+12, y+8, ST77XX_WHITE); // 右臂挥手
    tft.drawLine(x-3, y+10, x-8, y+15, ST77XX_WHITE); // 左腿
    tft.drawLine(x+3, y+10, x+8, y+15, ST77XX_WHITE); // 右腿
  }
}

void updateTime() {
  seconds++;
  if(seconds >= 60) {
    seconds = 0;
    minutes++;
    if(minutes >= 60) {
      minutes = 0;
      hours++;
      if(hours >= 24) {
        hours = 0;
      }
    }
  }
}

void updateWeather() {
  // 模拟天气变化
  static unsigned long lastWeatherUpdate = 0;
  if(millis() - lastWeatherUpdate > 30000) { // 每30秒更新一次天气
    lastWeatherUpdate = millis();
    
    // 随机变化温度±0.5度
    temperature += (random(0, 2) ? 0.5 : -0.5);
    temperature = constrain(temperature, 20.0, 35.0);
    
    // 随机变化湿度±5%
    humidity += random(-5, 6);
    humidity = constrain(humidity, 30, 90);
    
    // 随机改变天气状态
    String weatherTypes[] = {"Sunny", "Cloudy", "Clear", "Windy"};
    weatherStatus = weatherTypes[random(0, 4)];
  }
}

void updateAstronaut() {
  // 更新动画帧
  astronautFrame++;
  
  // 移动太空人
  if(astronautDirection) {
    astronautX += 2;
    if(astronautX > 110) {
      astronautDirection = false;
    }
  } else {
    astronautX -= 2;
    if(astronautX < 20) {
      astronautDirection = true;
    }
  }
  
  // 轻微上下浮动
  astronautY = 90 + sin(astronautFrame * 0.1) * 5;
}
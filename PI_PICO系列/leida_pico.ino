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

// 超声波传感器引脚
#define TRIG_PIN  2
#define ECHO_PIN  3

// 使用硬件SPI初始化ST7735
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// 屏幕尺寸
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// 雷达中心点
#define CENTER_X (TFT_WIDTH / 2)
#define CENTER_Y (TFT_HEIGHT / 2)

// 雷达参数
#define MAX_RADIUS 60
#define SWEEP_LENGTH 55

// 颜色定义
#define ST7735_GREEN 0x07E0
#define ST7735_RED   0xF800
#define ST7735_BLACK 0x0000

// 物体检测结构体
struct DetectedObject {
  bool active;
  int angle;
  int distance;
  unsigned long detectedTime;
};

DetectedObject currentObject = {false, 0, 0, 0};
const int DETECTION_THRESHOLD = 150; // 检测阈值150cm
const unsigned long OBJECT_DISPLAY_TIME = 3000; // 物体显示时间(ms)

// 扫描状态
int lastSweepAngle = -1;
bool backgroundDrawn = false;

void setup() {
  Serial.begin(115200);
  
  // 初始化超声波引脚
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // 初始化屏幕
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3); // 根据你的屏幕方向调整
  tft.fillScreen(ST7735_BLACK);
  
  // 初始化背光
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  Serial.println("Radar System Started");
}

void loop() {
  // 确保背景只绘制一次
  if (!backgroundDrawn) {
    drawRadarBackground();
    backgroundDrawn = true;
  }
  
  // 读取超声波距离
  int distance = readUltrasonicDistance();
  
  // 检测物体 - 简化逻辑
  detectObjectSimple(distance);
  
  // 更新雷达扫描
  updateRadarSweep();
  
  // 显示检测到的物体
  displayDetectedObject();
  
  delay(30); // 控制扫描速度
}

// 读取超声波距离 - 简化版本
int readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // 读取回声引脚，超时时间对应约10米
  long duration = pulseIn(ECHO_PIN, HIGH, 60000);
  
  // 如果超时或距离超过150cm，返回200表示无物体
  if (duration == 0) {
    return 200; // 无物体
  }
  
  // 计算距离
  int distance = duration * 0.034 / 2;
  
  // 如果距离超过150cm，返回200表示无物体
  if (distance > 150) {
    return 200; // 无物体
  }
  
  return distance;
}

// 简化的物体检测逻辑
void detectObjectSimple(int distance) {
  static unsigned long lastDebugTime = 0;
  
  // 每秒输出一次调试信息
  if (millis() - lastDebugTime > 1000) {
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print("cm, Active: ");
    Serial.println(currentObject.active);
    lastDebugTime = millis();
  }
  
  // 如果检测到物体且距离在阈值内
  if (distance <= DETECTION_THRESHOLD && distance >= 2) {
    if (!currentObject.active) {
      // 新物体检测
      currentObject.active = true;
      currentObject.distance = distance;
      currentObject.detectedTime = millis();
      currentObject.angle = lastSweepAngle;
      
      Serial.print("Object detected! Distance: ");
      Serial.print(distance);
      Serial.print("cm, Angle: ");
      Serial.print(currentObject.angle);
      Serial.println("°");
    } else {
      // 更新现有物体的距离
      currentObject.distance = distance;
    }
  } else {
    // 无物体或物体超出范围
    if (currentObject.active) {
      currentObject.active = false;
      Serial.println("Object out of range");
    }
  }
}

// 绘制静态雷达背景
void drawRadarBackground() {
  tft.fillScreen(ST7735_BLACK);
  
  // 绘制同心圆
  for (int r = 15; r <= MAX_RADIUS; r += 15) {
    tft.drawCircle(CENTER_X, CENTER_Y, r, ST7735_GREEN);
  }
  
  // 绘制十字线
  tft.drawFastVLine(CENTER_X, CENTER_Y - MAX_RADIUS, MAX_RADIUS * 2, ST7735_GREEN);
  tft.drawFastHLine(CENTER_X - MAX_RADIUS, CENTER_Y, MAX_RADIUS * 2, ST7735_GREEN);
  
  // 绘制方向标记
  tft.setCursor(CENTER_X - 5, CENTER_Y - MAX_RADIUS - 10);
  tft.setTextColor(ST7735_GREEN);
  tft.print("N");
  
  tft.setCursor(CENTER_X - 5, CENTER_Y + MAX_RADIUS + 2);
  tft.print("S");
  
  tft.setCursor(CENTER_X - MAX_RADIUS - 12, CENTER_Y - 5);
  tft.print("W");
  
  tft.setCursor(CENTER_X + MAX_RADIUS + 2, CENTER_Y - 5);
  tft.print("E");
  
  // 绘制距离标记
  tft.setCursor(CENTER_X + 20, CENTER_Y - 15);
  tft.print("30cm");
  tft.setCursor(CENTER_X + 20, CENTER_Y - 30);
  tft.print("60cm");
  tft.setCursor(CENTER_X + 20, CENTER_Y - 45);
  tft.print("90cm");
  
  Serial.println("Radar background drawn");
}

// 更新雷达扫描
void updateRadarSweep() {
  static int sweepAngle = 0;
  
  // 清除上一帧的扫描线
  if (lastSweepAngle != -1) {
    drawSweepLine(lastSweepAngle, ST7735_BLACK);
  }
  
  // 绘制当前扫描线
  drawSweepLine(sweepAngle, ST7735_GREEN);
  
  // 保存当前角度用于下一帧清除
  lastSweepAngle = sweepAngle;
  
  // 更新角度
  sweepAngle = (sweepAngle + 4) % 360;
}

// 绘制扫描线
void drawSweepLine(int angle, uint16_t color) {
  float rad = angle * PI / 180.0;
  int endX = CENTER_X + SWEEP_LENGTH * sin(rad);
  int endY = CENTER_Y - SWEEP_LENGTH * cos(rad);
  
  // 绘制扫描线
  tft.drawLine(CENTER_X, CENTER_Y, endX, endY, color);
  
  // 在扫描线末端绘制一个小点
  if (color == ST7735_GREEN) {
    tft.fillCircle(endX, endY, 2, color);
  }
}

// 显示检测到的物体
void displayDetectedObject() {
  static bool lastObjectState = false;
  static int lastAngle = 0;
  static int lastDistance = 0;
  
  if (currentObject.active) {
    // 计算物体位置
    float rad = currentObject.angle * PI / 180.0;
    int objectRadius = map(currentObject.distance, 2, DETECTION_THRESHOLD, 5, MAX_RADIUS - 5);
    int objX = CENTER_X + objectRadius * sin(rad);
    int objY = CENTER_Y - objectRadius * cos(rad);
    
    // 清除上一个物体位置（如果角度或距离发生变化）
    if (lastObjectState && (lastAngle != currentObject.angle || lastDistance != currentObject.distance)) {
      float lastRad = lastAngle * PI / 180.0;
      int lastObjX = CENTER_X + map(lastDistance, 2, DETECTION_THRESHOLD, 5, MAX_RADIUS - 5) * sin(lastRad);
      int lastObjY = CENTER_Y - map(lastDistance, 2, DETECTION_THRESHOLD, 5, MAX_RADIUS - 5) * cos(lastRad);
      tft.fillCircle(lastObjX, lastObjY, 4, ST7735_BLACK);
      // 重新绘制被覆盖的背景
      redrawBackgroundAt(lastObjX, lastObjY, 4);
    }
    
    // 绘制当前物体位置
    tft.fillCircle(objX, objY, 4, ST7735_RED);
    
    lastAngle = currentObject.angle;
    lastDistance = currentObject.distance;
  } else if (lastObjectState) {
    // 物体消失，清除显示
    float lastRad = lastAngle * PI / 180.0;
    int lastObjX = CENTER_X + map(lastDistance, 2, DETECTION_THRESHOLD, 5, MAX_RADIUS - 5) * sin(lastRad);
    int lastObjY = CENTER_Y - map(lastDistance, 2, DETECTION_THRESHOLD, 5, MAX_RADIUS - 5) * cos(lastRad);
    tft.fillCircle(lastObjX, lastObjY, 4, ST7735_BLACK);
    // 重新绘制被覆盖的背景
    redrawBackgroundAt(lastObjX, lastObjY, 4);
  }
  
  lastObjectState = currentObject.active;
}

// 在指定位置重新绘制背景（修复被物体覆盖的部分）
void redrawBackgroundAt(int x, int y, int radius) {
  // 检查是否在同心圆上
  for (int r = 15; r <= MAX_RADIUS; r += 15) {
    int distanceToCenter = sqrt(pow(x - CENTER_X, 2) + pow(y - CENTER_Y, 2));
    if (abs(distanceToCenter - r) <= radius + 1) {
      // 重新绘制这个圆的一小段
      redrawCircleSegment(r, x, y, radius);
    }
  }
  
  // 检查是否在十字线上
  if (abs(x - CENTER_X) <= radius + 1) {
    tft.drawFastVLine(CENTER_X, y - radius, radius * 2, ST7735_GREEN);
  }
  if (abs(y - CENTER_Y) <= radius + 1) {
    tft.drawFastHLine(x - radius, CENTER_Y, radius * 2, ST7735_GREEN);
  }
}

// 重新绘制圆的一小段
void redrawCircleSegment(int circleRadius, int x, int y, int affectedRadius) {
  // 计算受影响的角度范围
  float angleToPoint = atan2(y - CENTER_Y, x - CENTER_X);
  float angleRange = asin((float)affectedRadius / circleRadius);
  
  // 绘制受影响的一小段圆弧
  int startAngle = (angleToPoint - angleRange) * 180 / PI;
  int endAngle = (angleToPoint + angleRange) * 180 / PI;
  
  for (int angle = startAngle; angle <= endAngle; angle++) {
    float rad = angle * PI / 180.0;
    int px = CENTER_X + circleRadius * cos(rad);
    int py = CENTER_Y + circleRadius * sin(rad);
    if (px >= 0 && px < TFT_WIDTH && py >= 0 && py < TFT_HEIGHT) {
      tft.drawPixel(px, py, ST7735_GREEN);
    }
  }
}
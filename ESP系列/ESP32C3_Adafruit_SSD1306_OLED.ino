#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// 引脚定义
#define OLED_CS   7
#define OLED_DC   6
#define OLED_MOSI 3  // SDA
#define OLED_SCLK 2  // SCL
#define OLED_RST  -1  // 复位引脚，如果未连接则设为-1

// OLED显示参数
#define SCREEN_WIDTH 128  // OLED显示宽度，像素
#define SCREEN_HEIGHT 64  // OLED显示高度，像素

// 创建SSD1306显示对象（软件SPI）
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_SCLK, OLED_DC, OLED_RST, OLED_CS);

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32C3 SSD1306 OLED Test Start");
  
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
  display.println("ESP32-C3");
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
  
  // 测试2：不同字体大小
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Size 1: Text");
  display.setTextSize(2);
  display.println("Size 2");
  display.setTextSize(3);
  display.println("S3");
  display.display();
  delay(3000);
  
  // 测试3：图形绘制
  display.clearDisplay();
  
  // 绘制边框
  display.drawRect(0, 0, display.width()-1, display.height()-1, SSD1306_WHITE);
  
  // 绘制填充矩形
  display.fillRect(10, 10, 20, 20, SSD1306_WHITE);
  
  // 绘制圆形
  display.drawCircle(64, 32, 15, SSD1306_WHITE);
  
  // 绘制三角形
  display.drawTriangle(100, 10, 110, 30, 90, 30, SSD1306_WHITE);
  
  // 绘制线条
  for(int i=0; i<display.width(); i+=4) {
    display.drawPixel(i, display.height()-1, SSD1306_WHITE);
  }
  
  display.setCursor(5, 45);
  display.setTextSize(1);
  display.println("Graphics Test");
  
  display.display();
  delay(3000);
  
  // 测试4：进度条动画
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Progress Bar:");
  
  for(int i=0; i<=100; i+=5) {
    display.fillRect(0, 20, display.width(), 10, SSD1306_BLACK); // 清除区域
    display.drawRect(0, 20, display.width(), 10, SSD1306_WHITE); // 边框
    display.fillRect(2, 22, (display.width()-4)*i/100, 6, SSD1306_WHITE); // 进度条
    display.setCursor(50, 35);
    display.print(i);
    display.print("%");
    display.display();
    delay(100);
  }
  delay(1000);
  
  // 测试5：反色显示
  display.clearDisplay();
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // 反色文本
  display.fillRect(0, 0, display.width(), 16, SSD1306_WHITE);
  display.setCursor(5, 5);
  display.setTextSize(1);
  display.println("INVERT DISPLAY");
  
  display.setTextColor(SSD1306_WHITE); // 恢复正常颜色
  display.setCursor(0, 25);
  display.println("Normal Display");
  display.setCursor(0, 35);
  display.println("White on Black");
  display.display();
  delay(3000);
  
  // 测试6：显示系统信息
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("System Info:");
  display.print("Chip: ESP32-C3");
  display.setCursor(0, 20);
  display.print("Uptime: ");
  display.print(millis()/1000);
  display.println("s");
  display.setCursor(0, 30);
  display.print("Free RAM: ");
  display.print(esp_get_free_heap_size()/1024);
  display.println(" KB");
  display.setCursor(0, 40);
  display.print("Cycle: ");
  static int counter = 0;
  display.print(counter++);
  display.display();
  delay(3000);
  
  // 测试7：滚动文本
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("Scrolling");
  display.setTextSize(1);
  display.println("Text Demo");
  display.display();
  
  // 水平滚动
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(500);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(500);
  
  // 垂直滚动
  display.startscrolldiagright(0x00, 0x0F);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  
  // 测试8：动态图形
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Animation Test");
  
  // 移动的球
  for(int i=0; i<display.width()-10; i+=5) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Moving Ball");
    display.fillCircle(i, 40, 5, SSD1306_WHITE);
    display.display();
    delay(100);
  }
  
  // 呼吸灯效果
  for(int i=0; i<3; i++) {
    for(int j=0; j<=10; j++) {
      display.clearDisplay();
      display.fillRect(20, 20, j*2, j*2, SSD1306_WHITE);
      display.setCursor(60, 25);
      display.print("PWM: ");
      display.print(j*10);
      display.print("%");
      display.display();
      delay(50);
    }
    for(int j=10; j>=0; j--) {
      display.clearDisplay();
      display.fillRect(20, 20, j*2, j*2, SSD1306_WHITE);
      display.setCursor(60, 25);
      display.print("PWM: ");
      display.print(j*10);
      display.print("%");
      display.display();
      delay(50);
    }
  }
  
  Serial.println("Test cycle completed, restarting...");
}
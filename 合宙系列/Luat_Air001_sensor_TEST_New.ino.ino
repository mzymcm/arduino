#include <Air001_WS2812.h>
#include <Adafruit_ST7735.h>

// 合宙Air001迷你板 SPI 引脚定义

#define TFT_BL         -1
#define TFT_CS         PA4
#define TFT_DC         PA6
#define TFT_RST        -1
#define TFT_MOSI       PF0  // SDA
#define TFT_SCLK       PA5  // SCL


// OLED显示参数
#define SCREEN_WIDTH 128  // OLED显示宽度，像素
#define SCREEN_HEIGHT 64  // OLED显示高度，像素

// 创建SSD1306显示对象（软件SPI）
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         OLED_MOSI, OLED_SCLK, OLED_DC, OLED_RST, OLED_CS);
int sensorPin = PB2;  // 传感器连接至数字引脚2




void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT);
  Serial.println("Luat Air001 生物检测开始 ......");

  Air001_WS2812::begin();
  // 初始化OLED显示
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // 停止执行
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
  display.println("TEST Begin ...");
  display.display();
  delay(3000);
}

void loop() {
  int sensorState = digitalRead(sensorPin);  // 读取传感器状态

  if (sensorState == HIGH) {
    display.clearDisplay();
    Serial.println("有人经过！");
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Someone passed by here.");
    display.display();
    Air001_WS2812::setColor(Air001_WS2812::GREEN);

  } else {
    display.clearDisplay();
    Serial.println("一切正常");
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("All is well.");
    display.display();
    Air001_WS2812::off();
  }
  delay(500);  // 短暂延迟，避免串口数据刷新过快
}

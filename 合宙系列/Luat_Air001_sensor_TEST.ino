#include <Adafruit_SSD1306.h>


// 合宙Air001迷你板 SPI 引脚定义
#define OLED_CS PA4
#define OLED_DC PA6
#define OLED_MOSI PF0  // SDA
#define OLED_SCLK PA5  // SCL
#define OLED_RST -1    // 复位引脚，如果未连接则设为-1

#define LED_PIN PA7
#define NUM_LEDS 1  // 只控制1个LED以节省内存



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

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  // 关闭LED
  setColor(0, 255, 0);

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
    setColor(255, 255, 255);
  } else {
    display.clearDisplay();
    Serial.println("一切正常");
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("All is well.");
    display.display();
    setColor(0, 0, 0);
  }
  delay(500);  // 短暂延迟，避免串口数据刷新过快
}

void setColor(byte r, byte g, byte b) {
  // 发送GRB数据（WS2812使用GRB顺序）
  sendByte(g);
  sendByte(r);
  sendByte(b);
}
void sendByte(byte data) {
  for (int i = 0; i < 8; i++) {
    if (data & 0x80) {
      // 发送'1'
      digitalWrite(LED_PIN, HIGH);
      delayMicroseconds(0.7);
      digitalWrite(LED_PIN, LOW);
      delayMicroseconds(0.6);
    } else {
      // 发送'0'
      digitalWrite(LED_PIN, HIGH);
      delayMicroseconds(0.35);
      digitalWrite(LED_PIN, LOW);
      delayMicroseconds(0.8);
    }
    data <<= 1;
  }
}
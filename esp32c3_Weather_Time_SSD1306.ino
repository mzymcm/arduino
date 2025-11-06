#include <U8g2lib.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

// 引脚定义
#define OLED_CS 7
#define OLED_DC 6
#define OLED_MOSI 3  // SDA
#define OLED_SCLK 2  // SCL

// WiFi配置
const char* ssid = "CMCC-gyHQ";
const char* password = "2fw5f8g7";

// 全局变量优化
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(
  U8G2_R0,
  /* clock=*/OLED_SCLK,
  /* data=*/OLED_MOSI,
  /* cs=*/OLED_CS,
  /* dc=*/OLED_DC,
  /* reset=*/U8X8_PIN_NONE);

unsigned long lastTimeUpdate = 0;
unsigned long lastWeatherUpdate = 0;
const long timeInterval = 60000;
const long weatherInterval = 300000;  // 5分钟更新一次天气
bool wifiConnected = false;
bool timeSynced = false;
bool weatherUpdated = false;

// 预分配内存缓冲区
char timeStr[9];
char dateStr[20];
char weekStr[10];
char weatherDesc[20];
char temperatureStr[10];

void setup(void) {
  Serial.begin(115200);

  // 初始化OLED
  u8g2.begin();
  u8g2.enableUTF8Print();

  // 显示启动信息
  showStartupScreen();

  // 连接WiFi（非阻塞方式）
  startWiFiConnection();

  Serial.println("桌面天气时钟已启动!");
}

void loop(void) {
  unsigned long currentMillis = millis();

  // 处理WiFi连接状态
  handleWiFiConnection();

  // 显示时间
  displayTime();

  // 定期更新时间
  if (currentMillis - lastTimeUpdate >= timeInterval) {
    if (wifiConnected && !timeSynced) {
      updateTimeFromNTP();
    }
    lastTimeUpdate = currentMillis;
  }
  // 定期更新天气
  if (currentMillis - lastWeatherUpdate >= weatherInterval) {
    if (wifiConnected) {
      updateWeather();
    }
    lastWeatherUpdate = currentMillis;
  }

  delay(1000);
}

void showStartupScreen() {
  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_unifont_t_chinese1);
    u8g2.setCursor(0, 50);
    u8g2.print("Weather is begin...");
  } while (u8g2.nextPage());
}

void startWiFiConnection() {
  Serial.print("正在连接WiFi...");
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
}

void handleWiFiConnection() {
  static unsigned long lastWiFiCheck = 0;
  static int retryCount = 0;
  const unsigned long wifiCheckInterval = 5000;  // 5秒检查一次

  if (millis() - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = millis();

    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED && !wifiConnected) {
      wifiConnected = true;
      Serial.println("\nWiFi连接成功!");
      Serial.print("IP地址: ");
      Serial.println(WiFi.localIP());
      showWiFiStatus(true);

      // WiFi连接成功后立即同步时间
      updateTimeFromNTP();
      // WiFi连接成功后立即同步天气
      updateWeather();
    } else if (status != WL_CONNECTED && wifiConnected) {
      wifiConnected = false;
      timeSynced = false;
      Serial.println("\nWiFi连接断开!");
      showWiFiStatus(false);

      // 尝试重新连接
      if (retryCount < 10) {
        retryCount++;
        Serial.println("尝试重新连接...");
        startWiFiConnection();
      }
    } else if (status == WL_CONNECTED) {
      // WiFi保持连接状态
      retryCount = 0;
    }
  }
}

void showWiFiStatus(bool connected) {
  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_unifont_t_chinese1);
    u8g2.setCursor(0, 50);
    if (connected) {
      u8g2.print("登录 WiFi OK");
    } else {
      u8g2.print("登录 WiFi ERROR");
    }
  } while (u8g2.nextPage());
  delay(2000);
}

void updateTimeFromNTP() {
  if (!wifiConnected) return;

  Serial.println("开始时间同步...");
  configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.pool.ntp.org", "ntp1.aliyun.com");

  // 非阻塞方式等待时间同步
  for (int i = 0; i < 20; i++) {
    delay(500);
    time_t now = time(nullptr);
    if (now > 1000000000) {
      timeSynced = true;
      Serial.println("时间同步成功");
      showTimeSyncStatus(true);
      return;
    }
  }

  Serial.println("时间同步失败");
  showTimeSyncStatus(false);
}

void showTimeSyncStatus(bool success) {
  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_unifont_t_chinese1);
    u8g2.setCursor(0, 50);
    if (success) {
      u8g2.print("Time is onlin ok");
    } else {
      u8g2.print("Time is Error");
    }
  } while (u8g2.nextPage());
  delay(2000);
}

void displayTime() {
  time_t now = time(nullptr);
  // 显示时间
  struct tm* timeinfo = localtime(&now);
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  // 更新日期显示
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
           timeinfo->tm_year + 1900,
           timeinfo->tm_mon + 1,
           timeinfo->tm_mday);
  // 显示星期
  const char* weekdays[] = { "Sun[0]", "Mon[1]", "Tue[2]", "Wed[3]", "Thu[4]", "Fri[5]", "Sat[6]" };
  snprintf(weekStr, sizeof(weekStr), " : %s", weekdays[timeinfo->tm_wday]);

  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    // 显示WiFi状态指示器
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(96, 10);
    u8g2.print(wifiConnected ? "W >>>" : "X ...");

    if (now < 1000000000 || !timeSynced) {
      // 时间未同步
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(0, 50);
      u8g2.print("Time is geting...");
    } else {
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(1, 13);
      u8g2.print(timeStr);
      u8g2.setCursor(5, 30);
      u8g2.print(dateStr);
      u8g2.setCursor(70, 30);
      u8g2.print(weekStr);
    }

    // 显示天气信息（在屏幕右侧）
    if (weatherUpdated) {
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(5, 60);
      u8g2.print(convertWeatherToEnglish(weatherDesc));
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(60, 60);
      u8g2.print(temperatureStr);
    } else {
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(5, 60);
      u8g2.print("get weather...");
    }

  } while (u8g2.nextPage());
}


void updateWeather() {
  if (!wifiConnected) return;

  Serial.println("开始获取天气信息...");

  HTTPClient http;
  String url = "https://cn.wttr.in/阿拉善左旗?format=j1&lang=zh";

  http.begin(url);
  http.setTimeout(20000);  // 设置超时时间为10秒

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("天气数据获取成功");

    // 解析JSON数据
    if (parseWeatherData(payload)) {
      weatherUpdated = true;
      Serial.println("天气数据解析成功");
      showWeatherStatus(true);
    } else {
      Serial.println("天气数据解析失败");
      showWeatherStatus(false);
    }
  } else {
    Serial.printf("天气请求失败，错误代码: %d\n", httpCode);
    showWeatherStatus(false);
  }

  http.end();
}

bool parseWeatherData(String jsonString) {
  DynamicJsonDocument doc(4096);  // 根据JSON大小调整缓冲区

  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print("JSON解析失败: ");
    Serial.println(error.c_str());
    return false;
  }

  // 提取当前天气信息
  JsonObject current_condition = doc["current_condition"][0];

  if (!current_condition.isNull()) {
    // 获取天气描述
    const char* weatherDescRaw = current_condition["lang_zh"][0]["value"];
    if (weatherDescRaw) {
      strncpy(weatherDesc, weatherDescRaw, sizeof(weatherDesc) - 1);
      weatherDesc[sizeof(weatherDesc) - 1] = '\0';
    } else {
      strcpy(weatherDesc, "未知");
    }

    // 获取温度
    const char* tempC = current_condition["temp_C"];
    if (tempC) {
      snprintf(temperatureStr, sizeof(temperatureStr), "%s°C", tempC);
    } else {
      strcpy(temperatureStr, "--°C");
    }
    Serial.printf("天气: %s, 温度: %s\n", weatherDesc, temperatureStr);
    Serial.printf("WETH: %s, TMP: %s\n", convertWeatherToEnglish(weatherDesc), temperatureStr);
    return true;
  }

  return false;
}

void showWeatherStatus(bool success) {
  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_unifont_t_chinese1);
    u8g2.setCursor(0, 50);
    if (success) {
      u8g2.print("Weather Update OK");
    } else {
      u8g2.print("Weather Error");
    }
  } while (u8g2.nextPage());
  delay(2000);
}

String convertWeatherToEnglish(const String& chineseWeather) {
  struct WeatherMap {
    const char* chinese;
    const char* english;
  };

  static const WeatherMap weatherMap[] = {
    { "晴", "Sunny" }, { "晴朗", "Clear" }, { "晴天", "Sunny" }, { "多云", "Cloudy" }, { "少云", "P.Cloudy" }, { "阴", "Overcast" }, { "阴天", "Overcast" }, { "小雨", "L.Rain" }, { "中雨", "M.Rain" }, { "大雨", "H.Rain" }, { "暴雨", "Storm" }, { "雷阵雨", "T.Rain" }, { "雨", "Rain" }, { "小雪", "L.Snow" }, { "中雪", "M.Snow" }, { "大雪", "H.Snow" }, { "暴雪", "SnowStrm" }, { "雪", "Snow" }, { "雾", "Fog" }, { "浓雾", "D.Fog" }, { "薄雾", "Mist" }, { "雷", "Thunder" }, { "雷电", "Lightning" }, { "风", "Windy" }, { "大风", "Windy" }
  };

  for (const auto& mapping : weatherMap) {
    if (chineseWeather.indexOf(mapping.chinese) >= 0) {
      return mapping.english;
    }
  }

  return chineseWeather.length() > 0 ? chineseWeather : "N/A";
}

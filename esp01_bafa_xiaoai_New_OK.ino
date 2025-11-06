#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <time.h>

// ********************* 默认配置 **********************
struct Config {
  char ssid[32] = "";
  char pswd[32] = "";
  char uid[48] = "bfad90b7e71e450dace8f7a9f6fa38e6";
  char topic[32] = "6vmzpqvng002";
  char sleep_start[6] = "00:00";
  char sleep_end[6] = "22:00";
  uint8_t ap_timeout_minutes = 15;
  uint8_t led_pin = 2;
  bool configured = false;
};

Config config;

const char* host = "bemfa.com";
const int port = 8344;

const uint8_t AVAILABLE_PINS[] = {2, 4, 5,12,13,14,1,3};
const uint8_t AVAILABLE_PINS_COUNT = 8;

uint8_t currentLedPin = 4;

WiFiClient client;
ESP8266WebServer server(80);

// NTP服务器配置
const char* ntpServers[] = {
  "ntp.aliyun.com",
  "cn.ntp.org.cn",
  "ntp.tuna.tsinghua.edu.cn", 
  "s2c.time.edu.cn",
  "ntp.sjtu.edu.cn",
  "time.windows.com",
  "pool.ntp.org",
  "time.apple.com"
};
const int ntpServerCount = 8;
int currentNtpServer = 0;

// 时区配置 (东8区)
const int timeZone = 8 * 3600;
const int daylightOffset = 0;

#define EEPROM_SIZE 512
#define CONFIG_ADDR 0

struct RTCData {
  uint32_t crc32;
  bool ledState;
  bool inSleepMode;
  uint8_t wifiRetryCount;
  unsigned long lastSuccessfulTime;
  unsigned long lastSuccessfulMillis;
  uint8_t currentNtpIndex;
  unsigned long startupTime;
  bool timeEverSynced;
};

RTCData rtcData;

bool shouldSleep = false;
bool isAPMode = false;
bool timeSynced = false;
unsigned long lastHeartbeat = 0;
unsigned long lastTimeCheck = 0;
unsigned long lastWifiRetry = 0;
unsigned long lastTimeSync = 0;
unsigned long apModeStartTime = 0;
const int MAX_WIFI_RETRIES = 5;
unsigned long AP_MODE_TIMEOUT = 0;
unsigned long STARTUP_GRACE_PERIOD = 0;
const unsigned long TIME_SYNC_INTERVAL = 3600000;
const unsigned long TIME_SYNC_RETRY_INTERVAL = 300000;

// ********************* 函数声明 **********************
void loadConfig();
void saveConfig();
void setupWebServer();
void setupAP();
void switchToSTAMode();
void switchToAPMode();
bool setup_wifi();
bool syncTime();
bool syncTimeWithRetry();
bool getCurrentTime(int& hour, int& minute);
void connect_server();
void send_heartbeat();
void checkSleepTime();
void enterDeepSleep();
void saveRTCData();
uint32_t calculateCRC32(const uint8_t *data, size_t length);
String escapeHTML(String input);
String getSafeConfigValue(const char* value);
bool isValidPin(uint8_t pin);
void updateLedPin();
void printNetworkInfo();
bool validateTimeFormat(const char* timeStr);
void printDebugInfo();
String getFormattedTime();

// ********************* 函数实现 **********************

void printNetworkInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("╔══════════════════════════════════════╗");
    Serial.println("║           WiFi连接信息              ║");
    Serial.println("╠══════════════════════════════════════╣");
    Serial.print  ("║ SSID: ");
    Serial.print(config.ssid);
    for (int i = strlen(config.ssid); i < 30; i++) Serial.print(" ");
    Serial.println("║");
    Serial.print  ("║ IP地址: ");
    Serial.print(WiFi.localIP());
    String ipStr = WiFi.localIP().toString();
    for (int i = ipStr.length(); i < 28; i++) Serial.print(" ");
    Serial.println("║");
    Serial.print  ("║ 信号强度: ");
    Serial.print(WiFi.RSSI());
    Serial.print(" dBm");
    for (int i = String(WiFi.RSSI()).length() + 5; i < 27; i++) Serial.print(" ");
    Serial.println("║");
    Serial.println("╚══════════════════════════════════════╝");
  }
}

bool isValidPin(uint8_t pin) {
  for (int i = 0; i < AVAILABLE_PINS_COUNT; i++) {
    if (AVAILABLE_PINS[i] == pin) {
      return true;
    }
  }
  return false;
}

void updateLedPin() {
  if (currentLedPin != config.led_pin) {
    pinMode(currentLedPin, INPUT);
    currentLedPin = config.led_pin;
    pinMode(currentLedPin, OUTPUT);
    digitalWrite(currentLedPin, rtcData.ledState ? HIGH : LOW);
    Serial.print("LED引脚已切换到: GPIO");
    Serial.println(currentLedPin);
  }
}

void loadConfig() {
  EEPROM.get(CONFIG_ADDR, config);
  if (config.configured != true) {
    Serial.println("未找到有效配置");
    memset(config.ssid, 0, sizeof(config.ssid));
    memset(config.pswd, 0, sizeof(config.pswd));
    strcpy(config.uid, "bfad90b7e71e450dace8f7a9f6fa38e6");
    strcpy(config.topic, "6vmzpqvng002");
    strcpy(config.sleep_start, "00:00");
    strcpy(config.sleep_end, "22:00");
    config.ap_timeout_minutes = 15;
    config.led_pin = 4;
    config.configured = false;
  } else {
    Serial.println("从EEPROM加载配置成功");
    config.ssid[sizeof(config.ssid)-1] = '\0';
    config.pswd[sizeof(config.pswd)-1] = '\0';
    config.uid[sizeof(config.uid)-1] = '\0';
    config.topic[sizeof(config.topic)-1] = '\0';
    config.sleep_start[sizeof(config.sleep_start)-1] = '\0';
    config.sleep_end[sizeof(config.sleep_end)-1] = '\0';
    
    if (!isValidPin(config.led_pin)) {
      Serial.println("配置中的LED引脚无效，使用默认引脚4");
      config.led_pin = 4;
    }
  }
}

void saveConfig() {
  config.configured = true;
  EEPROM.put(CONFIG_ADDR, config);
  if (EEPROM.commit()) {
    Serial.println("配置已保存到EEPROM");
  } else {
    Serial.println("错误: 保存配置到EEPROM失败");
  }
}

String escapeHTML(String input) {
  input.replace("&", "&amp;");
  input.replace("<", "&lt;");
  input.replace(">", "&gt;");
  input.replace("\"", "&quot;");
  input.replace("'", "&#39;");
  return input;
}

String getSafeConfigValue(const char* value) {
  if (value == NULL || strlen(value) == 0) {
    return "";
  }
  return escapeHTML(String(value));
}

uint32_t calculateCRC32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

void saveRTCData() {
  rtcData.crc32 = calculateCRC32((uint8_t*)&rtcData + 4, sizeof(rtcData) - 4);
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));
}

bool validateTimeFormat(const char* timeStr) {
  if (strlen(timeStr) != 5) return false;
  if (timeStr[2] != ':') return false;
  if (!isdigit(timeStr[0]) || !isdigit(timeStr[1]) || !isdigit(timeStr[3]) || !isdigit(timeStr[4])) return false;
  
  int hour = atoi(timeStr);
  int minute = atoi(timeStr + 3);
  return (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59);
}

// 获取格式化的时间字符串
String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);
  return String(timeStr);
}

// 基于您提供的稳定方法优化的时间同步
bool syncTime() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("无法同步时间: WiFi未连接");
    return false;
  }

  Serial.println("开始时间同步...");
  
  // 使用当前NTP服务器
  const char* primaryServer = ntpServers[currentNtpServer];
  
  // 设置备用服务器
  const char* secondaryServer = ntpServers[(currentNtpServer + 1) % ntpServerCount];
  const char* tertiaryServer = ntpServers[(currentNtpServer + 2) % ntpServerCount];
  
  Serial.print("使用NTP服务器: ");
  Serial.print(primaryServer);
  Serial.print(", ");
  Serial.print(secondaryServer);
  Serial.print(", ");
  Serial.println(tertiaryServer);
  
  // 配置时间
  configTime(timeZone, daylightOffset, primaryServer, secondaryServer, tertiaryServer);

  // 非阻塞方式等待时间同步
  for (int i = 0; i < 30; i++) { // 增加到30次尝试，最多15秒
    delay(500);
    time_t now = time(nullptr);
    if (now > 1000000000) { // 检查时间是否合理（2001年之后）
      timeSynced = true;
      rtcData.lastSuccessfulTime = now;
      rtcData.lastSuccessfulMillis = millis();
      rtcData.timeEverSynced = true;
      saveRTCData();
      
      Serial.print("时间同步成功: ");
      Serial.println(getFormattedTime());
      return true;
    }
  }

  // 如果失败，切换到下一个服务器
  currentNtpServer = (currentNtpServer + 1) % ntpServerCount;
  Serial.println("时间同步失败，将尝试下一个服务器");
  return false;
}

// 带重试的时间同步
bool syncTimeWithRetry() {
  Serial.println("开始时间同步（带重试）...");
  
  for (int serverAttempt = 0; serverAttempt < ntpServerCount; serverAttempt++) {
    Serial.print("尝试NTP服务器 ");
    Serial.print(serverAttempt + 1);
    Serial.print("/");
    Serial.print(ntpServerCount);
    Serial.print(": ");
    Serial.println(ntpServers[currentNtpServer]);
    
    if (syncTime()) {
      return true;
    }
    
    // 短暂延迟后尝试下一个服务器
    if (serverAttempt < ntpServerCount - 1) {
      delay(2000);
    }
  }
  
  Serial.println("所有NTP服务器同步失败");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n=== MzyEsp8266 LED控制器启动 ===");
  
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();
  
  AP_MODE_TIMEOUT = config.ap_timeout_minutes * 60 * 1000;
  STARTUP_GRACE_PERIOD = config.ap_timeout_minutes * 60 * 1000;
  
  memset(&rtcData, 0, sizeof(rtcData));
  rtcData.ledState = false;
  rtcData.wifiRetryCount = 0;
  rtcData.lastSuccessfulTime = 0;
  rtcData.lastSuccessfulMillis = 0;
  rtcData.currentNtpIndex = 0;
  rtcData.startupTime = millis();
  rtcData.timeEverSynced = false;
  
  currentLedPin = config.led_pin;
  pinMode(currentLedPin, OUTPUT);
  digitalWrite(currentLedPin, LOW);
  
  setupWebServer();
  
  Serial.println("启动AP模式进行配置...");
  setupAP();
  apModeStartTime = millis();
  
  if (config.configured && strlen(config.ssid) > 0) {
    Serial.println("发现已有配置，后台尝试连接WiFi...");
    WiFi.begin(config.ssid, config.pswd);
  }
  
  Serial.println("启动完成 - 设备处于AP模式");
  Serial.print("注意：启动后");
  Serial.print(config.ap_timeout_minutes);
  Serial.println("分钟内不会进入休眠，方便配置");
  Serial.print("当前LED引脚: GPIO");
  Serial.println(currentLedPin);
}

void loop() {
  server.handleClient();
  
  if (isAPMode) {
    if (millis() - apModeStartTime > AP_MODE_TIMEOUT) {
      Serial.print("AP模式超时");
      Serial.print(config.ap_timeout_minutes);
      Serial.println("分钟，重启设备...");
      delay(1000);
      ESP.restart();
    }
    
    if (config.configured && strlen(config.ssid) > 0) {
      wl_status_t wifiStatus = WiFi.status();
      if (wifiStatus == WL_CONNECTED) {
        Serial.println("WiFi连接成功! 切换到STA模式...");
        printNetworkInfo();
        switchToSTAMode();
      } else if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL) {
        if (millis() - lastWifiRetry > 10000) {
          Serial.println("WiFi连接失败，请在AP模式下检查配置");
          lastWifiRetry = millis();
        }
      }
    }
    
    delay(100);
    return;
  }
  
  if (!config.configured || strlen(config.ssid) == 0) {
    Serial.println("没有有效配置，切换到AP模式...");
    switchToAPMode();
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi断开，尝试重新连接...");
    
    if (millis() - lastWifiRetry > 30000) {
      if (setup_wifi()) {
        Serial.println("WiFi重新连接成功!");
        printNetworkInfo();
        connect_server();
      } else {
        rtcData.wifiRetryCount++;
        saveRTCData();
        
        if (rtcData.wifiRetryCount >= MAX_WIFI_RETRIES) {
          Serial.println("WiFi连接失败次数过多，切换到AP模式...");
          switchToAPMode();
          return;
        }
      }
      lastWifiRetry = millis();
    }
  }
  
  // 时间同步逻辑
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long syncInterval = rtcData.timeEverSynced ? TIME_SYNC_INTERVAL : TIME_SYNC_RETRY_INTERVAL;
    
    if (millis() - lastTimeSync > syncInterval) {
      if (syncTimeWithRetry()) {
        lastTimeSync = millis();
        Serial.println("时间同步成功");
      } else {
        Serial.println("时间同步失败，将在5分钟后重试");
        lastTimeSync = millis() - (TIME_SYNC_INTERVAL - TIME_SYNC_RETRY_INTERVAL);
      }
    }
  }
  
  if (millis() - lastTimeCheck > 30000) {
    checkSleepTime();
    lastTimeCheck = millis();
    
    if (shouldSleep && WiFi.status() == WL_CONNECTED) {
      enterDeepSleep();
    }
  }
  
  if (client.connected()) {
    if (client.available()) {
      String message = client.readStringUntil('\n');
      Serial.print("收到消息: ");
      Serial.println(message);
      
      if (message.indexOf("on") != -1) {
        digitalWrite(currentLedPin, HIGH);
        rtcData.ledState = true;
        saveRTCData();
        Serial.println("LED开启");
      } else if (message.indexOf("off") != -1) {
        digitalWrite(currentLedPin, LOW);
        rtcData.ledState = false;
        saveRTCData();
        Serial.println("LED关闭");
      }
    }
    
    if (millis() - lastHeartbeat > 50000) {
      send_heartbeat();
      lastHeartbeat = millis();
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    connect_server();
  }
  
  delay(200);
}

void checkSleepTime() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  unsigned long currentRuntime = millis() - rtcData.startupTime;
  if (currentRuntime < STARTUP_GRACE_PERIOD) {
    shouldSleep = false;
    Serial.print("启动保护期内 (剩余");
    Serial.print((STARTUP_GRACE_PERIOD - currentRuntime) / 1000);
    Serial.println("秒)，暂不进入休眠");
    return;
  }
  
  int currentHour, currentMinute;
  if (!getCurrentTime(currentHour, currentMinute)) {
    Serial.println("时间获取失败，跳过休眠检查");
    return;
  }
  
  int currentTime = currentHour * 60 + currentMinute;
  
  if (!validateTimeFormat(config.sleep_start) || !validateTimeFormat(config.sleep_end)) {
    Serial.println("休眠时间格式无效，跳过休眠检查");
    shouldSleep = false;
    return;
  }
  
  int sleepStartHour = atoi(config.sleep_start);
  int sleepStartMinute = atoi(config.sleep_start + 3);
  int sleepStartTime = sleepStartHour * 60 + sleepStartMinute;
  
  int sleepEndHour = atoi(config.sleep_end);
  int sleepEndMinute = atoi(config.sleep_end + 3);
  int sleepEndTime = sleepEndHour * 60 + sleepEndMinute;
  
  if (sleepEndTime < sleepStartTime) {
    shouldSleep = (currentTime >= sleepStartTime) || (currentTime < sleepEndTime);
  } else {
    shouldSleep = (currentTime >= sleepStartTime) && (currentTime < sleepEndTime);
  }
  
  Serial.print("当前时间: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.print(currentMinute < 10 ? "0" : "");
  Serial.print(currentMinute);
  Serial.print(" - 休眠模式: ");
  Serial.println(shouldSleep ? "是" : "否");
}

void enterDeepSleep() {
  unsigned long currentRuntime = millis() - rtcData.startupTime;
  if (currentRuntime < STARTUP_GRACE_PERIOD) {
    Serial.println("启动保护期内，取消深度休眠");
    shouldSleep = false;
    return;
  }
  
  Serial.println("进入深度休眠...");
  
  rtcData.inSleepMode = true;
  saveRTCData();
  
  int currentHour, currentMinute;
  if (getCurrentTime(currentHour, currentMinute)) {
    int currentTime = currentHour * 60 + currentMinute;
    
    int sleepEndHour = atoi(config.sleep_end);
    int sleepEndMinute = atoi(config.sleep_end + 3);
    int sleepEndTime = sleepEndHour * 60 + sleepEndMinute;
    
    uint32_t sleepSeconds;
    if (sleepEndTime > currentTime) {
      sleepSeconds = (sleepEndTime - currentTime) * 60;
    } else {
      sleepSeconds = ((24 * 60 - currentTime) + sleepEndTime) * 60;
    }
    
    if (sleepSeconds > 71 * 60) {
      sleepSeconds = 71 * 60;
    }
    
    Serial.print("深度休眠 ");
    Serial.print(sleepSeconds);
    Serial.println(" 秒");
    
    ESP.deepSleep(sleepSeconds * 1000000);
  } else {
    Serial.println("使用默认休眠时间: 1小时");
    ESP.deepSleep(3600 * 1000000);
  }
}

bool getCurrentTime(int& hour, int& minute) {
  // 首先尝试获取实时时间
  time_t now = time(nullptr);
  if (now > 1000000000) {
    struct tm* timeinfo = localtime(&now);
    hour = timeinfo->tm_hour;
    minute = timeinfo->tm_min;
    return true;
  }
  
  // 如果NTP失败，使用RTC中保存的最后成功时间 + 经过的时间
  if (rtcData.lastSuccessfulTime > 0 && rtcData.lastSuccessfulMillis > 0) {
    unsigned long secondsSinceLastSync = (millis() - rtcData.lastSuccessfulMillis) / 1000;
    time_t currentEpoch = rtcData.lastSuccessfulTime + secondsSinceLastSync;
    
    struct tm* timeinfo = localtime(&currentEpoch);
    hour = timeinfo->tm_hour;
    minute = timeinfo->tm_min;
    
    Serial.println("使用缓存时间进行计算");
    return true;
  }
  
  Serial.println("无法获取有效时间");
  return false;
}

bool setup_wifi() {
  Serial.println();
  Serial.print("连接WiFi: ");
  Serial.println(config.ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  WiFi.begin(config.ssid, config.pswd);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
    
    if (attempts % 5 == 0) {
      Serial.print(" 尝试 ");
      Serial.print(attempts);
      Serial.print("/20, 状态: ");
      Serial.println(WiFi.status());
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi连接成功");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
    rtcData.wifiRetryCount = 0;
    
    printNetworkInfo();
    return true;
  } else {
    Serial.println("\nWiFi连接失败!");
    Serial.print("状态码: ");
    Serial.println(WiFi.status());
    return false;
  }
}

void setupAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("MzyEsp8266_LED", "12345678");
  isAPMode = true;
  Serial.println("AP模式已启动");
  Serial.print("AP IP地址: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("连接WiFi: MzyEsp8266_LED, 密码: 12345678");
  Serial.println("然后打开: http://" + WiFi.softAPIP().toString());
}

void switchToAPMode() {
  Serial.println("切换到AP模式...");
  WiFi.disconnect();
  delay(100);
  setupAP();
  apModeStartTime = millis();
  rtcData.wifiRetryCount = 0;
  saveRTCData();
}

void switchToSTAMode() {
  Serial.println("切换到STA模式...");
  isAPMode = false;
  WiFi.softAPdisconnect(true);
  delay(100);
  
  // 初始化时间同步
  if (syncTimeWithRetry()) {
    Serial.println("时间同步成功");
  }
  
  connect_server();
  
  Serial.println("现在处于STA模式，已连接到: " + String(config.ssid));
}

void connect_server() {
  Serial.print("连接巴法云服务器...");
  if (!client.connect(host, port)) {
    Serial.println(" 连接失败!");
    delay(2000);
    return;
  }
  
  Serial.println(" 连接成功!");
  String subscribeCmd = "cmd=1&uid=" + String(config.uid) + "&topic=" + String(config.topic) + "\r\n";
  client.print(subscribeCmd);
  Serial.println("订阅主题: " + String(config.topic));
}

void send_heartbeat() {
  String heartbeat = "cmd=0&msg=ping\r\n";
  client.print(heartbeat);
  Serial.println("心跳包已发送");
}

void printDebugInfo() {
  Serial.println("=== 调试信息 ===");
  Serial.print("WiFi状态: ");
  Serial.println(WiFi.status());
  Serial.print("时间同步状态: ");
  Serial.println(timeSynced ? "已同步" : "未同步");
  Serial.print("最后成功时间: ");
  Serial.println(rtcData.lastSuccessfulTime);
  Serial.print("当前NTP服务器: ");
  Serial.println(ntpServers[currentNtpServer]);
  Serial.print("当前时间: ");
  Serial.println(getFormattedTime());
  Serial.println("================");
}

// Web服务器设置保持不变...
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String currentTimeStr = getFormattedTime();
    if (currentTimeStr == "00:00:00" || currentTimeStr == "") {
      currentTimeStr = "未知";
    }
    
    unsigned long currentRuntime = millis() - rtcData.startupTime;
    String gracePeriodStatus = "已结束";
    if (currentRuntime < STARTUP_GRACE_PERIOD) {
      int remainingSeconds = (STARTUP_GRACE_PERIOD - currentRuntime) / 1000;
      int minutes = remainingSeconds / 60;
      int seconds = remainingSeconds % 60;
      gracePeriodStatus = String(minutes) + "分" + String(seconds) + "秒";
    }
    
    String pinOptions = "";
    for (int i = 0; i < AVAILABLE_PINS_COUNT; i++) {
      uint8_t pin = AVAILABLE_PINS[i];
      String pinDesc = "";
      if (pin == 2) pinDesc = " (GPIO2 - 设备异常时带电，要注意这方面)";
      else if (pin == 4) pinDesc = " (GPIO4 - I2C_SDA 有延迟)";
      else if (pin == 5) pinDesc = " (GPIO5 - I2C_SCL)";
      else if (pin == 12) pinDesc = " (GPIO12 - SPI_MISO)";
      else if (pin == 13) pinDesc = " (GPIO13 - SPI_MOSI)";
      else if (pin == 14) pinDesc = " (GPIO14 - SPI_SCLK)";
      else if (pin == 1) pinDesc = " (GPIO1 - TX 注意开关倒置)";
      else if (pin == 3) pinDesc = " (GPIO2 - RX)";
      
      String selected = (config.led_pin == pin) ? " selected" : "";
      pinOptions += "<option value='" + String(pin) + "'" + selected + ">GPIO" + String(pin) + pinDesc + "</option>";
    }
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>MzyEsp8266 LED控制器配置</title>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>";
    html += "body{font-family:'Microsoft YaHei',Arial,sans-serif;margin:20px;background:#f5f5f5;color:#333;}";
    html += ".container{max-width:500px;margin:0 auto;background:white;padding:25px;border-radius:12px;box-shadow:0 4px 15px rgba(0,0,0,0.1);}";
    html += ".form-group{margin:18px 0;}";
    html += "label{display:block;margin:8px 0;font-weight:bold;color:#444;font-size:14px;}";
    html += "input,select{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:6px;box-sizing:border-box;font-size:14px;transition:border 0.3s;}";
    html += "input:focus,select:focus{border-color:#4CAF50;outline:none;box-shadow:0 0 5px rgba(76,175,80,0.3);}";
    html += "button{background:linear-gradient(135deg,#4CAF50,#45a049);color:white;padding:12px 20px;border:none;border-radius:6px;cursor:pointer;width:100%;font-size:16px;font-weight:bold;transition:all 0.3s;}";
    html += "button:hover{background:linear-gradient(135deg,#45a049,#4CAF50);transform:translateY(-2px);box-shadow:0 4px 8px rgba(0,0,0,0.2);}";
    html += ".section{background:#f9f9f9;padding:18px;margin:18px 0;border-radius:8px;border-left:4px solid #4CAF50;}";
    html += "h1{color:#2c3e50;text-align:center;margin-bottom:25px;font-size:24px;}";
    html += "h3{color:#34495e;margin-top:0;font-size:16px;border-bottom:1px solid #eee;padding-bottom:8px;}";
    html += ".status-box{background:#e8f5e8;border:1px solid #4CAF50;padding:12px;border-radius:6px;margin:15px 0;}";
    html += ".warning-box{background:#fff3cd;border:1px solid #ffeaa7;padding:12px;border-radius:6px;margin:15px 0;}";
    html += ".info-box{background:#d1ecf1;border:1px solid #bee5eb;padding:12px;border-radius:6px;margin:15px 0;}";
    html += ".grace-box{background:#e8f4fd;border:1px solid #b3d9ff;padding:12px;border-radius:6px;margin:15px 0;}";
    html += ".control-links{margin:15px 0;text-align:center;}";
    html += ".control-links a{display:inline-block;margin:0 10px;padding:8px 16px;background:#3498db;color:white;text-decoration:none;border-radius:4px;transition:background 0.3s;}";
    html += ".control-links a:hover{background:#2980b9;}";
    html += ".control-links a.off{background:#e74c3c;}";
    html += ".control-links a.off:hover{background:#c0392b;}";
    html += ".control-links a.disabled{background:#95a5a6;cursor:not-allowed;}";
    html += ".control-links a.disabled:hover{background:#7f8c8d;}";
    html += ".status-message{margin-top:10px;padding:8px;border-radius:4px;text-align:center;font-weight:bold;display:none;}";
    html += ".status-success{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}";
    html += ".status-error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}";
    html += "</style>";
    
    html += "<script>";
    html += "function controlLED(cmd){";
    html += "var b1=document.getElementById('ledOn');";
    html += "var b2=document.getElementById('ledOff');";
    html += "var m=document.getElementById('statusMessage');";
    html += "var s=document.getElementById('ledStatus');";
    html += "b1.classList.add('disabled');";
    html += "b2.classList.add('disabled');";
    html += "m.style.display='block';";
    html += "m.className='status-message';";
    html += "m.textContent='发送指令中...';";
    html += "var x=new XMLHttpRequest();";
    html += "x.open('GET','/control?cmd='+cmd,true);";
    html += "x.onreadystatechange=function(){";
    html += "if(x.readyState===4){";
    html += "if(x.status===200){";
    html += "var r=JSON.parse(x.responseText);";
    html += "if(r.success){";
    html += "m.className='status-message status-success';";
    html += "m.textContent=r.message;";
    html += "s.textContent=r.ledState?'💡 开启':'🔌 关闭';";
    html += "}else{";
    html += "m.className='status-message status-error';";
    html += "m.textContent='操作失败: '+r.message;";
    html += "}}else{";
    html += "m.className='status-message status-error';";
    html += "m.textContent='网络错误';";
    html += "}";
    html += "setTimeout(function(){m.style.display='none';},3000);";
    html += "b1.classList.remove('disabled');";
    html += "b2.classList.remove('disabled');";
    html += "}};";
    html += "x.send();";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>📱 LED控制器配置</h1>";
    
    if (isAPMode) {
      html += "<div class='warning-box'>";
      html += "<strong>🔧 AP配置模式</strong><br>";
      html += "<strong>WiFi名称:</strong> MzyEsp8266_LED<br>";
      html += "<strong>密码:</strong> 12345678<br>";
      html += "<strong>配置地址:</strong> http://" + WiFi.softAPIP().toString();
      html += "</div>";
    }
    
    if (currentRuntime < STARTUP_GRACE_PERIOD) {
      html += "<div class='grace-box'>";
      html += "<strong>⏰ 启动保护期</strong><br>";
      html += "<strong>剩余时间:</strong> " + gracePeriodStatus + "<br>";
      html += "<strong>说明:</strong> 设备启动后" + String(config.ap_timeout_minutes) + "分钟内不会进入休眠模式，方便进行配置";
      html += "</div>";
    }
    
    html += "<form action='/save' method='POST'>";
    
    html += "<div class='section'>";
    html += "<h3>📶 WiFi设置</h3>";
    html += "<div class='form-group'>";
    html += "<label for='ssid'>WiFi名称 (SSID):</label>";
    html += "<input type='text' id='ssid' name='ssid' value='" + getSafeConfigValue(config.ssid) + "' placeholder='输入WiFi名称' required>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='pswd'>WiFi密码:</label>";
    html += "<input type='password' id='pswd' name='pswd' value='" + getSafeConfigValue(config.pswd) + "' placeholder='输入WiFi密码'>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h3>☁️ 巴法云设置</h3>";
    html += "<div class='form-group'>";
    html += "<label for='uid'>私钥UID:</label>";
    html += "<input type='text' id='uid' name='uid' value='" + getSafeConfigValue(config.uid) + "' placeholder='输入巴法云私钥UID' maxlength='32'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='topic'>主题名:</label>";
    html += "<input type='text' id='topic' name='topic' value='" + getSafeConfigValue(config.topic) + "' placeholder='输入主题名称' maxlength='15'>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h3>⚙️ 设备设置</h3>";
    html += "<div class='form-group'>";
    html += "<label for='ap_timeout_minutes'>AP模式超时时间 (分钟):</label>";
    html += "<input type='number' id='ap_timeout_minutes' name='ap_timeout_minutes' value='" + String(config.ap_timeout_minutes) + "' min='1' max='60' placeholder='1-60分钟'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='led_pin'>LED控制引脚:</label>";
    html += "<select id='led_pin' name='led_pin'>";
    html += pinOptions;
    html += "</select>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h3>💤 休眠时间设置</h3>";
    html += "<div class='form-group'>";
    html += "<label for='sleep_start'>休眠开始时间 (HH:MM):</label>";
    html += "<input type='text' id='sleep_start' name='sleep_start' value='" + getSafeConfigValue(config.sleep_start) + "' pattern='[0-9]{2}:[0-9]{2}' placeholder='例如: 22:00'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='sleep_end'>休眠结束时间 (HH:MM):</label>";
    html += "<input type='text' id='sleep_end' name='sleep_end' value='" + getSafeConfigValue(config.sleep_end) + "' pattern='[0-9]{2}:[0-9]{2}' placeholder='例如: 06:00'>";
    html += "</div>";
    html += "</div>";
    
    html += "<button type='submit'>💾 保存配置</button>";
    html += "</form>";
    
    html += "<div class='section'>";
    html += "<h3>📊 设备状态</h3>";
    html += "<div class='status-box'>";
    html += "<p><strong>运行模式:</strong> " + String(isAPMode ? "🔧 AP配置模式" : "📡 STA工作模式") + "</p>";
    html += "<p><strong>WiFi状态:</strong> " + String(WiFi.status() == WL_CONNECTED ? "✅ 已连接" : "❌ 未连接") + "</p>";
    if (!isAPMode && WiFi.status() == WL_CONNECTED) {
      html += "<p><strong>连接至:</strong> " + getSafeConfigValue(config.ssid) + "</p>";
      html += "<p><strong>IP地址:</strong> " + WiFi.localIP().toString() + "</p>";
    }
    html += "<p><strong>服务器连接:</strong> " + String(client.connected() ? "✅ 已连接" : "❌ 未连接") + "</p>";
    html += "<p><strong>LED状态:</strong> <span id='ledStatus'>" + String(rtcData.ledState ? "💡 开启" : "🔌 关闭") + "</span></p>";
    html += "<p><strong>LED引脚:</strong> GPIO" + String(config.led_pin) + "</p>";
    html += "<p><strong>当前时间:</strong> " + currentTimeStr + "</p>";
    html += "<p><strong>时间状态:</strong> " + String(timeSynced ? "✅ 已同步" : "⚠️ 未同步") + "</p>";
    html += "<p><strong>AP超时时间:</strong> " + String(config.ap_timeout_minutes) + "分钟</p>";
    html += "<p><strong>启动保护期:</strong> " + gracePeriodStatus + "</p>";
    html += "<p><strong>休眠状态:</strong> " + String(shouldSleep ? "💤 休眠中" : "⚡ 运行中") + "</p>";
    html += "<p><strong>WiFi重试次数:</strong> " + String(rtcData.wifiRetryCount) + "</p>";
    html += "<p><strong>当前NTP服务器:</strong> " + String(ntpServers[currentNtpServer]) + "</p>";
    html += "</div>";
    
    html += "<div class='control-links'>";
    html += "<a href='#' id='ledOn' onclick='controlLED(\"on\")'>开启LED</a>";
    html += "<a href='#' id='ledOff' class='off' onclick='controlLED(\"off\")'>关闭LED</a>";
    html += "<a href='/syncTime'>同步时间</a>";
    html += "<a href='/debug'>调试信息</a>";
    html += "</div>";
    
    html += "<div id='statusMessage' class='status-message'></div>";
    
    html += "<div style='text-align:center;margin-top:15px;'>";
    html += "<a href='/reboot' style='color:#7f8c8d;text-decoration:none;'>🔄 重启设备</a>";
    html += "</div>";
    html += "</div>";
    
    html += "</body></html>";
    server.send(200, "text/html; charset=UTF-8", html);
  });

  // 其他路由处理保持不变...
  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid")) {
      String ssid = server.arg("ssid");
      ssid.toCharArray(config.ssid, sizeof(config.ssid));
    }
    if (server.hasArg("pswd")) {
      String pswd = server.arg("pswd");
      pswd.toCharArray(config.pswd, sizeof(config.pswd));
    }
    if (server.hasArg("uid")) {
      String uid = server.arg("uid");
      uid.toCharArray(config.uid, sizeof(config.uid));
    }
    if (server.hasArg("topic")) {
      String topic = server.arg("topic");
      topic.toCharArray(config.topic, sizeof(config.topic));
    }
    if (server.hasArg("sleep_start")) {
      String sleep_start = server.arg("sleep_start");
      sleep_start.toCharArray(config.sleep_start, sizeof(config.sleep_start));
    }
    if (server.hasArg("sleep_end")) {
      String sleep_end = server.arg("sleep_end");
      sleep_end.toCharArray(config.sleep_end, sizeof(config.sleep_end));
    }
    if (server.hasArg("ap_timeout_minutes")) {
      config.ap_timeout_minutes = server.arg("ap_timeout_minutes").toInt();
      if (config.ap_timeout_minutes < 1) config.ap_timeout_minutes = 1;
      if (config.ap_timeout_minutes > 60) config.ap_timeout_minutes = 60;
    }
    if (server.hasArg("led_pin")) {
      uint8_t newPin = server.arg("led_pin").toInt();
      if (isValidPin(newPin)) {
        config.led_pin = newPin;
      }
    }
    
    saveConfig();
    updateLedPin();
    
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>配置保存</title>";
    html += "<style>body{font-family:'Microsoft YaHei',Arial;text-align:center;padding:50px;background:#f5f5f5;}";
    html += ".message{background:white;padding:30px;border-radius:10px;box-shadow:0 4px 15px rgba(0,0,0,0.1);display:inline-block;}</style></head><body>";
    html += "<div class='message'><h1>✅ 配置已保存</h1><p>设备将重启并应用新配置...</p></div></body></html>";
    server.send(200, "text/html; charset=UTF-8", html);
    
    delay(2000);
    ESP.restart();
  });

  server.on("/control", HTTP_GET, []() {
    String response = "";
    if (server.hasArg("cmd")) {
      String cmd = server.arg("cmd");
      if (cmd == "on") {
        digitalWrite(currentLedPin, HIGH);
        rtcData.ledState = true;
        saveRTCData();
        Serial.println("通过网页控制: LED开启");
        response = "{\"success\":true,\"message\":\"LED已开启\",\"ledState\":true}";
      } else if (cmd == "off") {
        digitalWrite(currentLedPin, LOW);
        rtcData.ledState = false;
        saveRTCData();
        Serial.println("通过网页控制: LED关闭");
        response = "{\"success\":true,\"message\":\"LED已关闭\",\"ledState\":false}";
      } else {
        response = "{\"success\":false,\"message\":\"未知命令\"}";
      }
    } else {
      response = "{\"success\":false,\"message\":\"缺少命令参数\"}";
    }
    
    server.send(200, "application/json", response);
  });

  server.on("/syncTime", HTTP_GET, []() {
    bool success = syncTimeWithRetry();
    String message = success ? "时间同步成功" : "时间同步失败";
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>时间同步</title>";
    html += "<style>body{font-family:'Microsoft YaHei',Arial;text-align:center;padding:50px;background:#f5f5f5;}";
    html += ".message{background:white;padding:30px;border-radius:10px;box-shadow:0 4px 15px rgba(0,0,0,0.1);display:inline-block;}</style></head><body>";
    html += "<div class='message'><h1>" + String(success ? "✅" : "❌") + " " + message + "</h1>";
    html += "<p><a href='/'>返回主页</a></p></div></body></html>";
    server.send(200, "text/html; charset=UTF-8", html);
  });

  server.on("/debug", HTTP_GET, []() {
    printDebugInfo();
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>调试信息</title>";
    html += "<style>body{font-family:'Microsoft YaHei',Arial;text-align:center;padding:50px;background:#f5f5f5;}";
    html += ".message{background:white;padding:30px;border-radius:10px;box-shadow:0 4px 15px rgba(0,0,0,0.1);display:inline-block;}</style></head><body>";
    html += "<div class='message'><h1>🔧 调试信息</h1>";
    html += "<p>调试信息已输出到串口监视器</p>";
    html += "<p><a href='/'>返回主页</a></p></div></body></html>";
    server.send(200, "text/html; charset=UTF-8", html);
  });

  server.on("/reboot", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>重启</title>";
    html += "<style>body{font-family:'Microsoft YaHei',Arial;text-align:center;padding:50px;background:#f5f5f5;}</style></head><body>";
    html += "<h1>🔄 重启中...</h1></body></html>";
    server.send(200, "text/html; charset=UTF-8", html);
    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println("HTTP服务器已启动");
}
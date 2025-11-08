#include <ESP8266WiFi.h>
#include <MQTT.h>

const char ssid[] = "CMCC-gyHQ";
const char pass[] = "2fw5f8g7";

WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;

// MQTT服务器配置
const char mqtt_broker[] = "192.168.1.39";
const char mqtt_topic[] = "test/topic";

void connect() {
  Serial.print("检查WiFi连接...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\n连接MQTT服务器...");
  while (!client.connect("ESP8266Client")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\n连接成功!");
  
  // 订阅主题（如果需要接收消息）
  client.subscribe(mqtt_topic);
}

void messageReceived(String &topic, String &payload) {
  Serial.println("收到消息: " + topic + " - " + payload);
}

void setup() {
  Serial.begin(115200);
  
  // 连接WiFi
  WiFi.begin(ssid, pass);
  
  // 配置MQTT客户端
  client.begin(mqtt_broker, net);
  client.onMessage(messageReceived);
  
  // 连接MQTT服务器
  connect();
}

void loop() {
  client.loop();
  delay(10); // 确保MQTT客户端正常运行

  if (!client.connected()) {
    connect();
  }

  // 每隔5秒发布一条消息
  if (millis() - lastMillis > 5000) {
    lastMillis = millis();
    
    String message = "Hello from ESP8266 - " + String(millis());
    client.publish(mqtt_topic, message);
    Serial.println("发布消息: " + message);
  }
}
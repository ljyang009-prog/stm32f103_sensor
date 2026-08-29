/*
 * ESP8266 烧录本程序（Arduino 固件），替代 AT 固件：
 *   - 自己连 WiFi
 *   - MQTT over TLS 连接 OneNET Studio
 *   - 从串口接收 STM32 发来的 "温度,湿度,气体,光照\r\n"，上报到物模型
 *
 * 依赖（Arduino IDE → 工具 → 管理库，搜索安装 PubSubClient）：
 *   - ESP8266WiFi（安装 ESP8266 板卡后自带）
 *   - PubSubClient
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

/* ===== WiFi ===== */
const char* WIFI_SSID = "OnePlus 11";
const char* WIFI_PASS = "ljy050724";

/* ===== OneNET（token / 一型一密）===== */
const char* ONENET_PROID   = "D155yb7UYS";   // 产品ID
const char* ONENET_DEVNAME = "sensor";       // 设备名称
const char* ONENET_TOKEN   = "version=2018-10-31&res=products%2FD155yb7UYS%2Fdevices%2Fsensor&et=1819377258&method=md5&sign=BIjuVa1khfEcMI%2F88Cm5IQ%3D%3D";

/* TLS 加密域名（注意是 mqttstls） */
const char*    MQTT_HOST = "D155yb7UYS.mqttstls.acc.cmcconenet.cn";
const uint16_t MQTT_PORT = 8883;

WiFiClientSecure net;
PubSubClient     mqtt(net);

char          pubTopic[96];
unsigned long msgId = 0;

void setup() {
  Serial.begin(115200);   // 与 STM32 串口通信
  delay(200);

  // 连 WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // TLS：跳过证书校验（测试用，省去证书配置）
  net.setInsecure();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  snprintf(pubTopic, sizeof(pubTopic), "$sys/%s/%s/thing/property/post",
           ONENET_PROID, ONENET_DEVNAME);
}

void mqttConnect() {
  // clientId=设备名称, username=产品ID, password=token
  while (!mqtt.connected()) {
    if (mqtt.connect(ONENET_DEVNAME, ONENET_PROID, ONENET_TOKEN)) {
      break;
    }
    delay(2000);
  }
}

/* 收到 STM32 一行 "温度,湿度,气体,光照"，上报到 OneNET */
void publishLine(char* line) {
  float t, h, g, l;
  if (sscanf(line, "%f,%f,%f,%f", &t, &h, &g, &l) != 4)
    return;

  char payload[256];
  snprintf(payload, sizeof(payload),
    "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{"
    "\"temperature\":{\"value\":%.1f},"
    "\"humidity\":{\"value\":%.1f},"
    "\"gas\":{\"value\":%.1f},"
    "\"light\":{\"value\":%.0f}}}",
    ++msgId, t, h, g, l);

  mqtt.publish(pubTopic, payload);
}

void loop() {
  if (!mqtt.connected()) {
    mqttConnect();
  }
  mqtt.loop();

  static char line[64];
  static int  idx = 0;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (idx > 0) {
        line[idx] = 0;
        publishLine(line);
        idx = 0;
      }
    } else if (idx < (int)sizeof(line) - 1) {
      line[idx++] = c;
    }
  }
}

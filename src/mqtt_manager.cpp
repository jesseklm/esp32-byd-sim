#include "mqtt_manager.h"

#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include <map>
#include <set>

#include "can_manager.h"
#include "config.h"
#include "main_vars.h"

PsychicMqttClient MqttManager::client;

String MqttManager::module_topic = mqtt_topic;
String MqttManager::will_topic;

unsigned long MqttManager::last_blink_time = 0;
unsigned long MqttManager::last_master_heartbeat_time = 0;

constexpr unsigned long MASTER_HEARTBEAT_TIMEOUT_MS = 10UL * 60UL * 1000UL;

struct QueuedMessage {
  String topic;
  String payload;
  bool retain;
  int priority;
};

struct MessageComparator {
  bool operator()(const QueuedMessage& a, const QueuedMessage& b) const {
    return a.priority > b.priority;  // high priority first
  }
};

static std::multiset<QueuedMessage, MessageComparator> messageQueue;

struct ValueConfig {
  float* valuePtr;
  float defaultValue;
};

static std::map<String, ValueConfig> valueMap = {
    {"limits/max_voltage", {&CanManager::limit_battery_voltage_max, max_cell_voltage* CanManager::number_of_cells}},
    {"limits/min_voltage", {&CanManager::limit_battery_voltage_min, min_cell_voltage* CanManager::number_of_cells}},
    {"limits/max_discharge_current", {&CanManager::limit_discharge_current_max, max_current}},
    {"limits/max_charge_current", {&CanManager::limit_charge_current_max, max_current}},
    {"battery/voltage", {&CanManager::battery_voltage, default_cell_voltage* CanManager::number_of_cells}},
    {"battery/current", {&CanManager::battery_current, 0.f}},
    {"battery/temp", {&CanManager::battery_temp, 12.f}},
    {"battery/max_cell_temp", {&CanManager::cell_temp_max, 13.f}},
    {"battery/min_cell_temp", {&CanManager::cell_temp_min, 11.f}},
    {"battery/soc", {&CanManager::soc_percent, 50.f}},
    {"battery/soh", {&CanManager::soh_percent, 100.f}},
    {"battery/remaining_capacity_ah", {&CanManager::remaining_capacity_ah, 80.f}},
    {"battery/full_capacity_ah", {&CanManager::full_capacity_ah, 160.f}},
};

void MqttManager::init() {
  if (module_topic.length() <= 0) {
    module_topic = MainVars::hostname + "/";
  }
  client.setServer(mqtt_server);
  client.setCredentials(mqtt_user, mqtt_password);
  will_topic = module_topic + "available";
  client.setWill(will_topic.c_str(), 0, true, "offline");
  client.setKeepAlive(60);
  client.onConnect(onConnect);
  client.onMessage(onMessage);
  client.connect();
  last_master_heartbeat_time = millis();
}

void MqttManager::loop() {
  if (millis() - last_blink_time < blink_time) {
    if ((millis() - last_blink_time) % 100 < 50) {
      digitalWrite(LED_BUILTIN, LED_OFF);
    } else {
      digitalWrite(LED_BUILTIN, LED_ON);
    }
  }
  unsigned long last_heartbeat = last_master_heartbeat_time;
  unsigned long delta = millis() - last_heartbeat;
  if (delta >= MASTER_HEARTBEAT_TIMEOUT_MS) {
    log(String("DEBUG check: delta=")+String(delta)+", limit="+String(MASTER_HEARTBEAT_TIMEOUT_MS), false);
    log(String(millis()) + " : " + String(last_heartbeat), false);
    log("master heartbeat timeout - restarting!", false);
    ESP.restart();
  }
  if (!client.connected() || messageQueue.empty()) {
    return;
  }
  const auto it = messageQueue.begin();
  const QueuedMessage& msg = *it;  // highest priority message
  client.publish(msg.topic.c_str(), 0, msg.retain, msg.payload.c_str(), 0, false);
  messageQueue.erase(it);
}

void MqttManager::otaUpdate(const String& path) {
  log(String("ota started [") + path + "] (" + millis() + ")", false);
  NetworkClientSecure secure_client;
  secure_client.setCACert(trustRoot);
  secure_client.setTimeout(12UL * 1000UL);
  httpUpdate.setLedPin(LED_BUILTIN, LED_ON);
  switch (httpUpdate.update(secure_client, String("https://") + ota_server + path)) {
    case HTTP_UPDATE_FAILED: {
      auto error_string = String("HTTP_UPDATE_FAILED Error (");
      error_string += httpUpdate.getLastError();
      error_string += "): ";
      error_string += httpUpdate.getLastErrorString();
      error_string += "\n";
      Serial.println(error_string);
      log(error_string, false);
    } break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      log("HTTP_UPDATE_NO_UPDATES", false);
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      log("HTTP_UPDATE_OK", false);
      break;
  }
}

void MqttManager::onMessage(char* topic, char* payload, int retain, int qos, bool dup) {
  String sTopic(topic);
  if (sTopic.startsWith(module_topic)) {
    sTopic = sTopic.substring(module_topic.length());
  }
  if (sTopic.equals("ota")) {
    otaUpdate(payload);
    return;
  }
  if (sTopic.equals("blink")) {
    last_blink_time = millis();
    return;
  }
  if (sTopic.equals(mqtt_master_heartbeat_topic)) {
    last_master_heartbeat_time = millis();
    return;
  }
  if (sTopic.equals("restart")) {
    log("restart requested - restarting!", false);
    ESP.restart();
  }
  const bool isSet = sTopic.endsWith("/set");
  const bool isReset = sTopic.endsWith("/reset");
  char* endPtr;
  const float value = strtof(payload, &endPtr);
  if (isSet && *endPtr != '\0') {
    log(String("failed to parse ") + payload + " of topic " + sTopic + ".");
    return;
  }
  if (!isSet && !isReset) return;
  if (isSet) {
    sTopic = sTopic.substring(0, sTopic.length() - 4);
  } else {
    sTopic = sTopic.substring(0, sTopic.length() - 6);
  }
  const auto it = valueMap.find(sTopic);
  if (it == valueMap.end()) {
    return;
  }
  if (isSet) {
    *it->second.valuePtr = value;
  } else {
    *it->second.valuePtr = it->second.defaultValue;
  }
}

void MqttManager::log(const String& line, const bool async, const int priority) {
  publish("log", line, false, async, priority);
}

void MqttManager::publish(const String& topic, const float value, const bool retain, const bool async,
                          const int priority) {
  publish(topic, String(value), retain, async, priority);
}

void MqttManager::publish(const String& topic, const uint32_t value, const bool retain, const bool async,
                          const int priority) {
  publish(topic, String(value), retain, async, priority);
}

void MqttManager::publish(const String& topic, const String& payload, const bool retain, const bool async,
                          const int priority) {
  if (async) {
    const QueuedMessage msg{module_topic + topic, payload, retain, priority};
    if (messageQueue.size() >= max_mqtt_send_queue) {
      if (const auto it = --messageQueue.end(); priority > it->priority) {
        messageQueue.erase(it);  // delete lowest priority message
        messageQueue.insert(msg);
      }
    } else {
      messageQueue.insert(msg);
    }
  } else {
    client.publish((module_topic + topic).c_str(), 0, retain, payload.c_str(), 0, false);
  }
}

void MqttManager::subscribe(const String& topic) { client.subscribe((module_topic + topic).c_str(), 0); }

void MqttManager::publishInfos() {
  // publish("version", VERSION, true);
  // publish("build_timestamp", BUILD_TIMESTAMP, true);
  publish("wifi", WiFi.SSID(), true, true, 20);
  publish("ip", WiFi.localIP().toString(), true, true, 40);
  publish("esp_sdk", ESP.getSdkVersion(), true, true, 20);
  publish("cpu",
          String(ESP.getChipModel()) + " rev " + ESP.getChipRevision() + " " + ESP.getChipCores() + "x" +
              ESP.getCpuFreqMHz() + "MHz",
          true, true, 20);
  publish("flash", String(ESP.getFlashChipSize() / 1024 / 1024) + " MiB, Mode: " + ESP.getFlashChipMode(), true, true,
          20);
  publish("heap", String(ESP.getHeapSize() / 1024) + " KiB", true, true, 20);
  publish("psram", String(ESP.getPsramSize() / 1024) + " KiB", true, true, 20);
  // publish("build_time", unixToTime(CURRENT_TIME), true);
}

void MqttManager::onConnect(bool session_present) {
  Serial.println("connected");
  publish("available", "online", true, true, 100);
  publish("hostname", MainVars::hostname, true, true, 20);
  publish("module_topic", module_topic, true, true, 20);
  publishInfos();
  client.subscribe(mqtt_master_heartbeat_topic, 0);
  subscribe("+/+/set");
  subscribe("+/+/reset");
  subscribe("restart");
  // subscribe("debug");
  subscribe("blink");
  subscribe("ota");
}

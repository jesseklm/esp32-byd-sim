#include <Arduino.h>

#include "can_manager.h"
#include "config.h"
#include "main_vars.h"
#include "mqtt_manager.h"
#include "wifi_manager.h"

void setup() {
  Serial.begin(74880);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_ON);

  WifiManager::connect();
  WifiManager::syncTime();

  MqttManager::init();
  CanManager::init();

  digitalWrite(LED_BUILTIN, LED_OFF);
}

void loop() {
  MqttManager::loop();
  CanManager::loop();
}

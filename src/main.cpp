#include <Arduino.h>

#include "can_manager.h"
#include "config.h"
#include "main_vars.h"
#include "mqtt_manager.h"
#include "wifi_manager.h"

// [[noreturn]] void mqttTask(void* pvParameters) {
//   for (;;) {
//     MqttManager::loop();
//     delay(1);
//   }
// }

// [[noreturn]] void canTask(void* pvParameters) {
//   for (;;) {
//     CanManager::loop();
//     delay(1);
//   }
// }

void setup() {
  Serial.begin(74880);
  while (!Serial);
  delay(2000);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_ON);

  // pinMode(CAN_RX_PIN, OUTPUT);
  // pinMode(CAN_TX_PIN, INPUT);

  WifiManager::connect();
  WifiManager::syncTime();

  MqttManager::init();
  CanManager::init();

  digitalWrite(LED_BUILTIN, LED_OFF);

  // xTaskCreate(mqttTask, "mqtt", 8192, nullptr, 5, nullptr);
  // xTaskCreate(canTask, "can", 8192, nullptr, 6, nullptr);
  // vTaskDelete(nullptr);
}

void loop() {
  MqttManager::loop();
  CanManager::loop();
}

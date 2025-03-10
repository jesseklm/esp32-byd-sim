#pragma once

#include <Arduino.h>

constexpr const char* mqtt_topic = "master/can/";
constexpr const char* mqtt_master_heartbeat_topic = "master/uptime";
constexpr unsigned int max_mqtt_send_queue = 100;
constexpr unsigned int heartbeat_timeout = 120'000;
constexpr unsigned int blink_time = 5'000;

#define LED_ON HIGH
#define LED_OFF LOW

#ifdef LOLIN_C3_MINI
#define CAN_INT_PIN 8
#elif defined(LOLIN_S2_MINI)
#define CAN_INT_PIN 33
#endif

class MainVars {
 public:
  static String device_type;
  static String mac_address;
  static String hostname;

 private:
  static String getMacAddress();
};

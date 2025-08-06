#pragma once

#include <Arduino.h>

constexpr uint8_t fw_major_version = 0x03;
constexpr uint8_t fw_minor_version = 0x16;

constexpr const char* mqtt_topic = "master/can/";
constexpr const char* mqtt_master_heartbeat_topic = "master/uptime";
constexpr unsigned int max_mqtt_send_queue = 100;

constexpr unsigned int blink_time = 5U * 1000U;

constexpr unsigned long heartbeat_timeout_limits_ms = 2UL * 60UL * 1000UL;
constexpr unsigned long heartbeat_timeout_reboot_ms = 10UL * 60UL * 1000UL;

constexpr unsigned long sntp_timeout_ms = 10UL * 1000UL;
constexpr unsigned long wifi_timeout_ms = 30UL * 1000UL;
constexpr unsigned int min_time_s = 24U * 60U * 60U;

#define LED_ON HIGH
#define LED_OFF LOW

#ifdef LOLIN_C3_MINI
constexpr uint8_t can_int_pin = 8;
#elif defined(LOLIN_S2_MINI)
constexpr uint8_t can_tx_pin = 35;
constexpr uint8_t can_rx_pin = 9;
#endif

class MainVars {
 public:
  static String device_type;
  static String mac_address;
  static String hostname;

 private:
  static String getMacAddress();
};

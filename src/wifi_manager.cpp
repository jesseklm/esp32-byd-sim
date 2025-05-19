#include "wifi_manager.h"

#include <Arduino.h>

#include "config.h"
#include "main_vars.h"
#include "mqtt_manager.h"

constexpr uint32_t sntp_timeout_ms = 10 * 1000;
constexpr time_t min_time_s = 24 * 3600;

void WifiManager::connect() {
  Serial.println();
  Serial.printf("Connecting to %s.", ssid);
  WiFi.persistent(false);
  WiFi.softAPdisconnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFiClass::mode(WIFI_STA);
  WiFiClass::hostname(MainVars::hostname);
  WiFi.begin(ssid, password);
#ifdef LOLIN_C3_MINI_V1
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  const String localIP = WiFi.localIP().toString();
  Serial.printf("IP Address: %s\n", localIP.c_str());

  for (int i = 0; i < 2; i++) {
    const String dns = WiFi.dnsIP(i).toString();
    Serial.printf("DNS%d: %s\n", i + 1, dns.c_str());
  }
}

void WifiManager::syncTime() {
  Serial.print("Setting time using SNTP.");
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "0.de.pool.ntp.org", "1.de.pool.ntp.org", "2.de.pool.ntp.org");
  time_t now = time(nullptr);
  uint32_t start = millis();
  while (now < min_time_s && millis() - start < sntp_timeout_ms) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  if (now < min_time_s) {
    Serial.println("\nSNTP-Sync timeout!");
    return;
  }
  Serial.println("\nTime synchronized!");
  tm timeInfo{};
  getLocalTime(&timeInfo);
  char current_time_buffer[32];
  strftime(current_time_buffer, sizeof(current_time_buffer), "%c", &timeInfo);
  Serial.printf("Current time: %s", current_time_buffer);
  MqttManager::publish("last_boot", current_time_buffer, true);
}

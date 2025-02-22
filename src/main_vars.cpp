#include "main_vars.h"

#include <esp_mac.h>
#include <WiFi.h>


String MainVars::getMacAddress() {
  uint8_t mac[6];
  esp_base_mac_addr_get(mac);
  char mac_string[6 * 2 + 1] = {};
  snprintf(mac_string, sizeof(mac_string), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return {mac_string};
}

String MainVars::device_type = String("espcan");
String MainVars::mac_address = getMacAddress();
String MainVars::hostname = device_type + '-' + mac_address;

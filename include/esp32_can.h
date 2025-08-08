#pragma once

#include <Arduino.h>

class ESP32Can {
 public:
  static bool init();
  static bool send(uint32_t id, uint8_t len, const uint8_t* buf);
  static void loop();
};

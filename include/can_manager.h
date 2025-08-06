#pragma once

#include <Arduino.h>

#include <map>

struct ValueConfig {
  float* valuePtr;
  float defaultValue;
};

class CanManager {
 public:
  static constexpr uint32_t CAN_EXTENDED = 0x80000000;
  static constexpr uint32_t CAN_REMOTE_REQUEST = 0x40000000;

  static void init();
  static bool send(uint32_t id, uint8_t len, uint8_t *buf);
  static void loop();

  static std::map<String, ValueConfig> value_map;

  static float number_of_cells;

  static float limit_battery_voltage_max;
  static float limit_battery_voltage_min;
  static float limit_discharge_current_max;
  static float limit_charge_current_max;

  static float battery_voltage;
  static float battery_current;
  static float battery_temp;

  static float cell_temp_max;
  static float cell_temp_min;

  static float soc_percent;
  static float soh_percent;
  static float remaining_capacity_ah;
  static float full_capacity_ah;

 private:
  static bool init_failed;
  static unsigned long last_successful_send;
  static unsigned long last_send_2s;
  static unsigned long last_send_10s;
  static unsigned long last_send_60s;
  static void readMessages();
  static void readMessage();
  static void sendLimits();
  static void sendStates();
  static void sendBatteryInfo();
  static void sendCellInfo();
  static void sendAlarm();
};

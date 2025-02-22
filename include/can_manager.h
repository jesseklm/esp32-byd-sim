#pragma once

#include <mcp_can.h>

#define CAN_EXTENDED 0x80000000
#define CAN_REMOTE_REQUEST 0x40000000

class CanManager {
 public:
  static void init();
  static bool send(INT32U id, INT8U len, INT8U *buf);
  static void loop();

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
  static MCP_CAN can;
  static const uint8_t CAN_INT;
  static bool init_failed;
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

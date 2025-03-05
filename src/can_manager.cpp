#include "can_manager.h"

#include <Arduino.h>

#include "config.h"
#include "main_vars.h"
#include "mqtt_manager.h"

float CanManager::number_of_cells = static_cast<float>(battery_modules * battery_cells_per_module);

float CanManager::limit_battery_voltage_max;
float CanManager::limit_battery_voltage_min;
float CanManager::limit_discharge_current_max;
float CanManager::limit_charge_current_max;

float CanManager::battery_voltage;
float CanManager::battery_current = 0.f;
float CanManager::battery_temp = 12.f;

float CanManager::cell_temp_max = 13.f;
float CanManager::cell_temp_min = 11.f;

float CanManager::soc_percent = 50.f;
float CanManager::soh_percent = 100.f;
float CanManager::remaining_capacity_ah = 80.f;
float CanManager::full_capacity_ah = 160.f;

const uint8_t CanManager::CAN_INT = CAN_INT_PIN;

MCP_CAN CanManager::can(SS);

unsigned long CanManager::last_send_2s = 0;
unsigned long CanManager::last_send_10s = 0;
unsigned long CanManager::last_send_60s = 0;

bool CanManager::init_failed = false;

struct Message {
  unsigned long id;
  byte data[8];
};

const Message initMessages[] = {
    {0x250,
     {fw_major_version, fw_minor_version, 0x00, 0x66, static_cast<uint8_t>((battery_wh_max / 100) >> 8),
      static_cast<uint8_t>(battery_wh_max / 100), 0x02, 0x09}},
    {0x290, {0x06, 0x37, 0x10, 0xD9, 0x00, 0x00, 0x00, 0x00}},
    {0x2D0, {0x00, 'B', 'Y', 'D', 0x00, 0x00, 0x00, 0x00}},
    {0x3D0, {0x00, 'B', 'a', 't', 't', 'e', 'r', 'y'}},
    {0x3D0, {0x01, '-', 'B', 'o', 'x', ' ', 'P', 'r'}},
    {0x3D0, {0x02, 'e', 'm', 'i', 'u', 'm', ' ', 'H'}},
    {0x3D0, {0x03, 'V', 'S', 0x00, 0x00, 0x00, 0x00, 0x00}},
};

template <typename T>
void setBytes(uint8_t* data, const size_t start, T value) {
  const auto value_bytes = reinterpret_cast<uint8_t*>(&value);
  for (size_t i = 0; i < sizeof(T); i++) {
    data[start + i] = value_bytes[sizeof(T) - i - 1];  // big-endian
  }
}

template <typename T>
T getValue(const uint8_t* data, const size_t start) {
  T value{};
  const auto value_bytes = reinterpret_cast<uint8_t*>(&value);
  for (size_t i = 0; i < sizeof(T); ++i) {
    value_bytes[sizeof(T) - 1 - i] = data[start + i];  // big-endian
  }
  return value;
}

void CanManager::init() {
  limit_battery_voltage_max = max_cell_voltage * number_of_cells;
  limit_battery_voltage_min = min_cell_voltage * number_of_cells;
  limit_discharge_current_max = max_current;
  limit_charge_current_max = max_current;
  battery_voltage = default_cell_voltage * number_of_cells;
  if (can.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
  } else {
    Serial.println("Error Initializing MCP2515...");
    MqttManager::log("Error Initializing MCP2515...");
    init_failed = true;
  }
  can.setMode(MCP_NORMAL);
  pinMode(CAN_INT, INPUT);
}

bool CanManager::send(INT32U id, INT8U len, INT8U* buf) {
  digitalWrite(LED_BUILTIN, LED_ON);
  Serial.printf("send: Standard ID: 0x%.3lX       DLC: %1d  Data:", id, len);
  for (byte i = 0; i < len; i++) {
    Serial.printf(" 0x%.2X", buf[i]);
  }
  Serial.println();
  auto result = can.sendMsgBuf(id, len, buf);
  digitalWrite(LED_BUILTIN, LED_OFF);
  if (result == CAN_OK) {
    return true;
  } else if (result == CAN_GETTXBFTIMEOUT) {
    Serial.println("Error sending - get tx buff time out!");
    MqttManager::log("Error sending - get tx buff time out!");
  } else if (result == CAN_SENDMSGTIMEOUT) {
    Serial.println("Error sending - send msg timeout!");
    MqttManager::log("Error sending - send msg timeout!");
  } else {
    Serial.println("Error sending - unknown error!");
    MqttManager::log("Error sending - unknown error!");
  }
  return false;
}

void CanManager::loop() {
  if (init_failed) {
    if (millis() >= 300'000) {
      ESP.restart();
    }
    return;
  }
  readMessages();
  if (millis() - last_send_2s >= 2'000) {
    last_send_2s = millis();
    sendLimits();
  }
  if (millis() - last_send_10s >= 10'000) {
    last_send_10s = millis();
    sendCellInfo();
    sendBatteryInfo();
    sendStates();
  }
  if (millis() - last_send_60s >= 60'000) {
    last_send_60s = millis();
    sendAlarm();
  }
}

void CanManager::sendLimits() {
  if (millis() - MqttManager::last_master_heartbeat_time >= heartbeat_timeout) {
    limit_discharge_current_max = 0;
    limit_charge_current_max = 0;
    MqttManager::log("Master Heartbeat missed!");
  }
  byte data[8]{};
  setBytes(data, 0, static_cast<uint16_t>(limit_battery_voltage_max * 10.f));   // 230V max
  setBytes(data, 2, static_cast<uint16_t>(limit_battery_voltage_min * 10.f));   // 170V min
  setBytes(data, 4, static_cast<int16_t>(limit_discharge_current_max * 10.f));  // 25,6A max discharge
  setBytes(data, 6, static_cast<int16_t>(limit_charge_current_max * 10.f));     // 25,6A max charge
  if (send(0x110, 8, data)) {
    MqttManager::publish("limits/max_voltage", limit_battery_voltage_max);
    MqttManager::publish("limits/min_voltage", limit_battery_voltage_min);
    MqttManager::publish("limits/max_discharge_current", limit_discharge_current_max);
    MqttManager::publish("limits/max_charge_current", limit_charge_current_max);
  }
}

void CanManager::sendBatteryInfo() {
  byte data[8]{};
  setBytes(data, 0, static_cast<int16_t>(battery_voltage * 10.f));  // 215V battery voltage
  setBytes(data, 2, static_cast<int16_t>(battery_current * 10.f));  // 4,3A battery current
  setBytes(data, 4, static_cast<int16_t>(battery_temp * 10.f));     // 22°C battery temp
  setBytes(data, 6, static_cast<int16_t>(776));                     // ???
  if (send(0x1d0, 8, data)) {
    MqttManager::publish("battery/voltage", battery_voltage);
    MqttManager::publish("battery/current", battery_current);
    MqttManager::publish("battery/temp", battery_temp);
  }
}

void CanManager::sendCellInfo() {
  byte data[8]{};
  setBytes(data, 0, static_cast<uint16_t>(cell_temp_max * 10.f));  // 23°C max cell temp
  setBytes(data, 2, static_cast<uint16_t>(cell_temp_min * 10.f));  // 22°C min cell temp
  if (send(0x210, 8, data)) {
    MqttManager::publish("battery/max_cell_temp", cell_temp_max);
    MqttManager::publish("battery/min_cell_temp", cell_temp_min);
  }
}

void CanManager::sendStates() {
  remaining_capacity_ah = soc_percent / 100 * full_capacity_ah;  // calculate remaining_capacity_ah by soc
  byte data[8]{};
  setBytes(data, 0, static_cast<uint16_t>(soc_percent * 100.f));           // 28,7% soc % Vrfd
  setBytes(data, 2, static_cast<uint16_t>(soh_percent * 100.f));           // 100% soh % Vrfd
  setBytes(data, 4, static_cast<uint16_t>(remaining_capacity_ah * 10.f));  // remaining capacity 1/10Ah
  setBytes(data, 6, static_cast<uint16_t>(full_capacity_ah * 10.f));       // fully charged capacity 1/10Ah
  if (send(0x150, 8, data)) {
    MqttManager::publish("battery/soc", soc_percent);
    MqttManager::publish("battery/soh", soh_percent);
    MqttManager::publish("battery/remaining_capacity_ah", remaining_capacity_ah);
    MqttManager::publish("battery/full_capacity_ah", full_capacity_ah);
  }
}

void CanManager::sendAlarm() {
  byte data[8]{};
  send(0x190, 8, data);
}

void CanManager::readMessages() {
  if (!digitalRead(CAN_INT)) {
    unsigned long start_time = millis();
    while (millis() - start_time <= 100) {
      while (!digitalRead(CAN_INT)) {
        readMessage();
      }
    }
    uint8_t eflg = can.getError();
    if (eflg & MCP_EFLG_RX1OVR || eflg & MCP_EFLG_RX0OVR) {
      Serial.println("buffer overflow!");
      MqttManager::log("buffer overflow!");
      can.resetOverflowErrors();
    }
  }
}

void CanManager::readMessage() {
  unsigned long rxId;
  unsigned char len = 0;
  unsigned char rxBuf[9];
  // char msgString[128];
  // If CAN0_INT pin is low, read receive buffer
  can.readMsgBuf(&rxId, &len, rxBuf);
  // Read data: len = data length, buf = data byte(s)
  Serial.print("recv: ");

  if ((rxId & CAN_EXTENDED) == CAN_EXTENDED)
    // Determine if ID is standard (11 bits) or extended (29 bits)
    Serial.printf("Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
  else
    Serial.printf("Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);

  if ((rxId & CAN_REMOTE_REQUEST) == CAN_REMOTE_REQUEST) {
    // Determine if message is a remote request frame.
    Serial.print(" REMOTE REQUEST FRAME");
  } else {
    for (byte i = 0; i < len; i++) {
      Serial.printf(" 0x%.2X", rxBuf[i]);
    }
    Serial.println();
  }

  if (rxId == 0x91) {
    const auto wr_battery_voltage = getValue<uint16_t>(rxBuf, 0);
    const auto wr_battery_current = getValue<uint16_t>(rxBuf, 2);
    const auto wr_temperature = getValue<uint16_t>(rxBuf, 4);
    MqttManager::publish("inverter/battery_voltage", static_cast<float>(wr_battery_voltage) * 0.1f);
    MqttManager::publish("inverter/battery_current", static_cast<float>(wr_battery_current) * 0.1f);
    MqttManager::publish("inverter/temperature", static_cast<float>(wr_temperature) * 0.1f);
  } else if (rxId == 0xd1) {
    const auto wr_soc = getValue<uint16_t>(rxBuf, 0);
    MqttManager::publish("inverter/soc", static_cast<float>(wr_soc) * 0.1f);
  } else if (rxId == 0x111) {
    const auto wr_timestamp = getValue<uint32_t>(rxBuf, 0);
    MqttManager::publish("inverter/timestamp", wr_timestamp);
  } else if (rxId == 0x151 && rxBuf[0] == 0x0) {
    rxBuf[len] = '\0';
    auto inverter_name = String(reinterpret_cast<char*>(rxBuf + 1));
    if (inverter_name.length() > 0) {
      MqttManager::publish("inverter/type", String(reinterpret_cast<char*>(rxBuf + 1)), true);
    }
  } else if (rxId == 0x151 && rxBuf[0] == 0x1) {
    MqttManager::log("sending initMessages!");
    for (const auto& [id, data] : initMessages) {
      for (int attempts = 0; attempts < 3 && !send(id, 8, const_cast<byte*>(data)); attempts++) {
        delay(3);
      }
    }
  } else {
    // MqttManager::log(fullLog);
  }
}

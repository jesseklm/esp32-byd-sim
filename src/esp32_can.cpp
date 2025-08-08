#include "esp32_can.h"

#include "can_manager.h"
#include "main_vars.h"
#include "mqtt_manager.h"

bool ESP32Can::init() {
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)can_tx_pin, (gpio_num_t)can_rx_pin, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver installed");
  } else {
    Serial.println("Failed to install driver");
    MqttManager::log("Failed to install can driver...");
    return false;
  }
  if (twai_start() == ESP_OK) {
    Serial.println("Driver started");
  } else {
    Serial.println("Failed to start driver");
    MqttManager::log("Failed to start can driver...");
    return false;
  }
  constexpr uint32_t alerts_to_enable =
      TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL;
  if (twai_reconfigure_alerts(alerts_to_enable, nullptr) == ESP_OK) {
    Serial.println("CAN Alerts reconfigured");
  } else {
    Serial.println("Failed to reconfigure alerts");
    return false;
  }
  return true;
}

bool ESP32Can::send(const uint32_t id, const uint8_t len, const uint8_t* buf) {
  Serial.printf("send: Standard ID: 0x%.3lX       DLC: %1d  Data:", id, len);
  for (byte i = 0; i < len; i++) {
    Serial.printf(" 0x%.2X", buf[i]);
  }
  Serial.println();
  twai_message_t message;
  message.identifier = id;
  message.data_length_code = len;
  for (int i = 0; i < len; i++) {
    message.data[i] = buf[i];
  }
  message.extd = 0;
  message.rtr = 0;

  // Queue message for transmission
  const auto result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    return true;
  }
  Serial.print("Failed to queue message for transmission: ");
  Serial.println(result);
  MqttManager::log("Failed to queue can message for transmission" + result);
  return false;
}

void ESP32Can::loop() {
  uint32_t alerts_triggered;
  twai_read_alerts(&alerts_triggered, 0);
  twai_status_info_t twaistatus;
  twai_get_status_info(&twaistatus);
  if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
    Serial.println("Alert: TWAI controller has become error passive.");
  }
  if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
    Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
    Serial.printf("Bus error count: %d\n", twaistatus.bus_error_count);
  }
  if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
    Serial.println("Alert: The RX queue is full causing a received frame to be lost.");
    Serial.printf("RX buffered: %d\t", twaistatus.msgs_to_rx);
    Serial.printf("RX missed: %d\t", twaistatus.rx_missed_count);
    Serial.printf("RX overrun %d\n", twaistatus.rx_overrun_count);
  }
  if (alerts_triggered & TWAI_ALERT_RX_DATA) {
    twai_message_t message;
    while (twai_receive(&message, 0) == ESP_OK) {
      CanManager::readMessage(message);
    }
  }
}

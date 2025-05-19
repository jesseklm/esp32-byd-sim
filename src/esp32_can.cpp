#include "esp32_can.h"

#include <driver/twai.h>

#include "main_vars.h"
#include "mqtt_manager.h"

bool ESP32Can::init() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
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
    return true;
}

bool ESP32Can::send(uint32_t id, uint8_t len, uint8_t* buf) {
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
    auto result = twai_transmit(&message, pdMS_TO_TICKS(1000));
    // const auto result = can.sendMsgBuf(id, len, buf);
    if (result == ESP_OK) {
      return true;
    } else {
      Serial.println("Failed to queue message for transmission");
      Serial.println(result);
      MqttManager::log(String("Failed to queue can message for transmission") + result);
    }
    // if (result == CAN_OK) {
    //   last_successful_send = millis();
    //   return true;
    // }
    // if (result == CAN_GETTXBFTIMEOUT) {
    //   Serial.println("Error sending - get tx buff time out!");
    //   MqttManager::log("Error sending - get tx buff time out!");
    //   if (millis() - last_successful_send >= 120'000) {
    //     ESP.restart();
    //   }
    // } else if (result == CAN_SENDMSGTIMEOUT) {
    //   Serial.println("Error sending - send msg timeout!");
    //   MqttManager::log("Error sending - send msg timeout!");
    // } else {
    //   Serial.println("Error sending - unknown error!");
    //   MqttManager::log("Error sending - unknown error!");
    // }
    return false;
}

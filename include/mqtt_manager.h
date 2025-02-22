#pragma once

#include <Arduino.h>
#include <PsychicMqttClient.h>


class MqttManager
{
public:
    static void init();
    static void loop();
    static void log(const String& line, bool async = true);
    static void publish(const String& topic, float value, bool retain = false, bool async = true);
    static void publish(const String& topic, uint32_t value, bool retain = false, bool async = true);
    static void publish(const String& topic, const String& payload, bool retain = false, bool async = true);
    static void subscribe(const String& topic);
    static void publishInfos();

private:
    static PsychicMqttClient client;
    static String module_topic;
    static String will_topic;
    static unsigned long last_blink_time;
    static void onConnect(bool session_present);
    static void onMessage(char* topic, char* payload, int retain, int qos, bool dup);
    static void otaUpdate(const String& path);
};

#pragma once

#include <Arduino.h>

#define LED_ON HIGH
#define LED_OFF LOW

class MainVars
{
public:
    static String device_type;
    static String mac_address;
    static String hostname;

private:
    static String getMacAddress();
};

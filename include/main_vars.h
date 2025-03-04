#pragma once

#include <Arduino.h>

#define LED_ON HIGH
#define LED_OFF LOW

#ifdef LOLIN_C3_MINI
#define CAN_INT_PIN 8
#elif defined(LOLIN_S2_MINI)
#define CAN_INT_PIN 33
#endif

class MainVars
{
public:
    static String device_type;
    static String mac_address;
    static String hostname;

private:
    static String getMacAddress();
};

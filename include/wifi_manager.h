#pragma once

#include <IPAddress.h>

class WifiManager
{
public:
    static void connect();
    static void syncTime();

private:
    static IPAddress getDnsServer(int index);
};

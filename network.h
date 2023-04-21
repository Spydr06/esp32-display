#pragma once

namespace network {
    const char* ssid PROGMEM = "Keller";
    const char* password PROGMEM = "?Es1tlubHiJb!";

    const char* server_name PROGMEM = "esp-display"; // access http://esp-display in browser
}

#include <WiFiUdp.h>
#include <NTPClient.h>

namespace ntp {
    WiFiUDP udp_socket;
    NTPClient client(udp_socket, "pool.ntp.org");

    uint16_t time_offset = 7200;
}

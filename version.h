#ifndef ESP32_DISPLAY_VERSION_H
#define ESP32_DISPLAY_VERSION_H

#define ESP32_DISPLAY_VERSION_MAJOR 0
#define ESP32_DISPLAY_VERSION_MINOR 1
#define ESP32_DISPLAY_BUILD_DATE __DATE__
#define ESP32_DISPLAY_BUILD_TIME __TIME__

#define ESP32_DISPLAY_VERSION \
    QUOTE_EXPAND(ESP32_DISPLAY_VERSION_MAJOR) "." \
    QUOTE_EXPAND(ESP32_DISPLAY_VERSION_MINOR)

#endif /* ESP32_DISPLAY_VERSION_H */

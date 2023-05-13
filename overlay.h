#pragma once

namespace overlay {
    #define DIGIT_WIDTH 3
    #define DIGIT_HEIGHT 5 
    const uint16_t DIGITS[12] = {
        // <space>
        0b000000000000000,
        // 0
        0b111101101101111,
        // 1
        0b110010010010111,
        // 2
        0b111001111100111,
        // 3
        0b111001111001111,
        // 4
        0b101101111001001,
        // 5
        0b111100111001111,
        // 6
        0b111100111101111,
        // 7
        0b111101001010010,
        // 8
        0b111101111101111,
        // 9
        0b111101111001011,
        // :
        0b000010000010000,
    };

    inline const uint16_t* get_digit(char c) {
        return &DIGITS[isdigit(c) ? c - '0' + 1 : c == ':' ? 11 : 0];
    }

    inline void clock(uint16_t x, uint16_t y) {
        display::dma->drawChar(x + 10, y, ':', display::WHITE, display::BLACK, 1);
        
        char buf[3];

        int hour = ntp::client.getHours();
        snprintf(buf, 3, "%02d", hour);

        display::dma->drawChar(x + 0, y, buf[0], display::WHITE, display::BLACK, 1);
        display::dma->drawChar(x + 6, y, buf[1], display::WHITE, display::BLACK, 1);

        // :

        int minute = ntp::client.getMinutes();
        snprintf(buf, 3, "%02d", minute);

        display::dma->drawChar(x + 14, y, buf[0], display::WHITE, display::BLACK, 1);
        display::dma->drawChar(x + 20, y, buf[1], display::WHITE, display::BLACK, 1);
    }

    void draw() {
       // display.setBrightness(OVERLAY_BRIGHTNESS);
       // clock(4, 4);
    }
}

#pragma once

#include "display.h"
#include "bitmap.h"

namespace overlay {
    #define DIGIT_WIDTH 3
    #define DIGIT_HEIGHT 5 

    char hour[3] = "--";
    char minute[3] = "--";

    using namespace bitmap;

    const Bitmap DIGITS[10] = {
        (Bitmap){
            .width = 3,
            .height = 5,
            .data = (uint8_t[]){0b11110110, 0b1101111}
        }
    };

    //inline const uint16_t* get_digit(char c) {
    //    return &DIGITS[isdigit(c) ? c - '0' + 1 : c == ':' ? 11 : 0];
    //}

    inline void clock(uint16_t x, uint16_t y) {
        display::dma->drawChar(x + 10, y, ':', display::WHITE, display::BLACK, 1);

        display::dma->drawChar(x + 0, y, hour[0], display::WHITE, display::BLACK, 1);
        display::dma->drawChar(x + 6, y, hour[1], display::WHITE, display::BLACK, 1);
        display::dma->drawChar(x + 14, y, minute[0], display::WHITE, display::BLACK, 1);
        display::dma->drawChar(x + 20, y, minute[1], display::WHITE, display::BLACK, 1);
    }

    void draw() {
        clock(2, 2);
    }
}

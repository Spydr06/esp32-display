#pragma once

#include "display.h"

namespace bitmap {
    struct Bitmap {
        uint8_t width;
        uint8_t height;
        uint8_t* data;
    };

    void show_bitmap(const Bitmap* bitmap, int16_t color) {
        display::dma->fillScreen(display::BLACK);

        uint8_t x = (PANEL_RES_X - bitmap->width) / 2;
        uint8_t y = (PANEL_RES_Y - bitmap->height) / 2;
        
        display::dma->drawBitmap(x, y, bitmap->data, bitmap->width, bitmap->height, color);
    }

    void show_bitmap(uint8_t x, uint8_t y, const Bitmap* bitmap, int16_t color) {
        display::dma->drawBitmap(x, y, bitmap->data, bitmap->width, bitmap->height, color);
    }
}

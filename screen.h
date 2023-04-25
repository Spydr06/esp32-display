#pragma once


#define PxMATRIX_MAX_WIDTH 64
#define PxMATRIX_MAX_HEIGHT 32
#define PxMATRIX_double_buffer true

#include <PxMatrix.h>
#include <stdint.h>

#define SCREEN_WIDTH  PxMATRIX_MAX_WIDTH
#define SCREEN_HEIGHT PxMATRIX_MAX_HEIGHT

#define SCREEN_UPDATE_TIME 100

#ifdef ESP8266
    #include <Ticker.h>
    
    #define P_LAT 16
    #define P_A 5
    #define P_B 4
    #define P_C 15
    #define P_D 12
    #define P_E 0
    #define P_OE 2

    Ticker display_ticker;
#else
    #error "Only ESP8266 cpus are supported currently"
#endif

PxMATRIX display(SCREEN_WIDTH, SCREEN_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D);

#ifdef ESP8266
    void display_updater() {
        display.display(SCREEN_UPDATE_TIME);
    }
#endif

const int16_t WHITE_COLOR = display.color565(255, 255, 255);
const int16_t BLACK_COLOR = display.color565(0, 0, 0);

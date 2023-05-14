#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

#define PANEL_PIN_A 18
#define PANEL_PIN_R1 23
#define PANEL_PIN_G1 22
#define PANEL_PIN_B1 21
#define PANEL_PIN_R2 0
#define PANEL_PIN_G2 2
#define PANEL_PIN_CLK 14

namespace display {
    MatrixPanel_I2S_DMA* dma = nullptr;

    const uint16_t WHITE = dma->color565(255, 255, 255);
    const uint16_t BLACK = dma->color565(0, 0, 0);
    const uint16_t RED = dma->color565(255, 100, 100);

    void init(void) {
        HUB75_I2S_CFG mx_config(
            PANEL_RES_X,
            PANEL_RES_Y,
            PANEL_CHAIN
        );

        mx_config.gpio.a = PANEL_PIN_A;
        mx_config.gpio.r1 = PANEL_PIN_R1;
        mx_config.gpio.g1 = PANEL_PIN_G1;
        mx_config.gpio.b1 = PANEL_PIN_B1;
        mx_config.gpio.r2 = PANEL_PIN_R2;
        mx_config.gpio.g2 = PANEL_PIN_G2;
        mx_config.gpio.clk = PANEL_PIN_CLK;

        dma = new MatrixPanel_I2S_DMA(mx_config);
        dma->begin();
        dma->setBrightness(255);
        dma->clearScreen();
        dma->setLatBlanking(2);
    }
}



#pragma once
#ifdef HOSYOND_CYD40
#include <Arduino.h>
#include <TFT_eSPI.h>
namespace HosyondBattery {
struct Reading {
    uint16_t mv;
    uint8_t pct;
    bool present;
    bool chargingEstimated;
};
const Reading& sample();
void drawChargingBolt(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color);
}
#endif

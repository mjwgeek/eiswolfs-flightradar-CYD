#include "auto_brightness.h"
#include "config.h"
#include "settings_store.h"

namespace AutoBrightness {

namespace {
    // -1 = noch kein Messwert vorhanden (nur direkt nach dem Booten, vor
    // dem allerersten begin()-Aufruf).
    float smoothedRaw = -1.0f;
    uint8_t lastPercent = Config::BRIGHTNESS_MIN_PERCENT;

    // Bildet den geglaetteten ADC-Rohwert (Config::AUTO_BRIGHTNESS_ADC_MIN..
    // MAX, siehe dortiger Kommentar zu den Schaetzwerten) linear auf
    // Config::BRIGHTNESS_MIN_PERCENT..MAX_PERCENT ab.
    uint8_t percentFromRaw(float raw) {
        float clamped = raw;
        if (clamped < (float)Config::AUTO_BRIGHTNESS_ADC_MIN) clamped = (float)Config::AUTO_BRIGHTNESS_ADC_MIN;
        if (clamped > (float)Config::AUTO_BRIGHTNESS_ADC_MAX) clamped = (float)Config::AUTO_BRIGHTNESS_ADC_MAX;
        float fraction = (clamped - Config::AUTO_BRIGHTNESS_ADC_MIN) /
                          (float)(Config::AUTO_BRIGHTNESS_ADC_MAX - Config::AUTO_BRIGHTNESS_ADC_MIN);
        return (uint8_t)(Config::BRIGHTNESS_MIN_PERCENT +
                          fraction * (Config::BRIGHTNESS_MAX_PERCENT - Config::BRIGHTNESS_MIN_PERCENT));
    }

    void sample() {
        int raw = analogRead(Config::LDR_PIN);
        if (smoothedRaw < 0.0f) {
            smoothedRaw = (float)raw; // erster Messwert - sofort uebernehmen, nichts zu glaetten
        } else {
            smoothedRaw += ((float)raw - smoothedRaw) * Config::AUTO_BRIGHTNESS_SMOOTHING;
        }
        lastPercent = percentFromRaw(smoothedRaw);
    }
}

void begin() {
#ifdef HOSYOND_CYD40
    // GPIO34 measures battery voltage, not ambient light.
    lastPercent = Config::BRIGHTNESS_MAX_PERCENT;
#else
    pinMode(Config::LDR_PIN, INPUT);
    sample();
#endif
}

void update() {
#ifndef HOSYOND_CYD40
    if (!SettingsStore::autoBrightnessEnabled()) return;
    sample();
#endif
}

uint8_t currentPercent() { return lastPercent; }

}

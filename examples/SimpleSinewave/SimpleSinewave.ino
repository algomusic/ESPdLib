/*
 * SimpleSinewave - ESPdLib Example
 *
 * Plays a sine wave with frequency and amplitude control from two
 * potentiometers.
 *
 * Patch signal flow:
 *   [r freq] -> [osc~] -> [*~ amp] -> [dac~]
 *
 * Hardware:
 *   - ESP32 board
 *   - I2S DAC (e.g., MAX98357A or PCM5102) on pins BCLK=38, WS=39, DOUT=40
 *   - Potentiometer on GPIO1 (frequency: 50-2000 Hz)
 *   - Potentiometer on GPIO2 (amplitude: 0-1)
 *
 * Upload the data/ folder to LittleFS first:
 *   Cmd+Shift+P -> "Upload LittleFS to Pico/ESP8266/ESP32"
 */

#include <ESPdLib.h>

#define POT_FREQ_PIN 1
#define POT_AMP_PIN  2

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("SimpleSinewave - ESPdLib Example");

    ESPdLib::Config config;
    config.bclkPin = 38;
    config.wsPin = 39;
    config.doutPin = 40;

    if (!Pd.begin(config)) {
        Serial.println("ESPdLib init failed!");
        while (1) delay(1000);
    }

    Pd.onPrint([](const char* msg) { Serial.printf("[Pd] %s", msg); });

    if (!Pd.openPatch("simple-sinewave.pd")) {
        Serial.println("Failed to open patch!");
        while (1) delay(1000);
    }

    Pd.sendFloat("freq", 220.0);
    Pd.sendFloat("amp", 0.3);

    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
    float freq = 50.0 + (analogRead(POT_FREQ_PIN) / 4095.0) * 1950.0;
    float amp = analogRead(POT_AMP_PIN) / 4095.0;

    Pd.sendFloat("freq", freq);
    Pd.sendFloat("amp", amp);

    delay(20);
}

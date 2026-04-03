/*
 * ASineTone - ESPdLib Minimal Example
 *
 * Plays a 440 Hz sine wave through I2S using the sine-tone.pd patch.
 * Upload the data/ folder to LittleFS first:
 *   Cmd+Shift+P -> "Upload LittleFS to Pico/ESP8266/ESP32"
 *
 * Hardware:
 *   - ESP32 board
 *   - I2S DAC (e.g., MAX98357A or PCM5102) on pins BCLK=38, WS=39, DOUT=40
 */

#include <ESPdLib.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    ESPdLib::Config config;
    config.bclkPin = 38;
    config.wsPin = 39;
    config.doutPin = 40;

    if (!Pd.begin(config)) {
        Serial.println("ESPdLib init failed!");
        while (1) delay(1000);
    }

    if (!Pd.openPatch("sine-tone.pd")) {
        Serial.println("Failed to open patch!");
        while (1) delay(1000);
    }

    Serial.println("Playing 440 Hz sine tone");
}

void loop() {
    delay(1000);
}

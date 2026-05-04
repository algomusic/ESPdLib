/*
 * ASineTone - ESPdLib Minimal Example
 *
 * Plays a 440 Hz sine wave through I2S using the sine-tone.pd patch.
 * Upload the data/ folder to LittleFS first:
 *   Cmd+Shift+P -> "Upload LittleFS to Pico/ESP8266/ESP32"
 *   Ensure that no Arduino sketch has the Serial Monitor open
 *   Older ESP32s may require a slower upload speed
 *
 * Hardware:
 *   - ESP32 board
 *   - I2S DAC (e.g., MAX98357A or PCM5102) on pins BCLK=38, WS=39, DOUT=40
 *   - or internal DAC on original ESP32 on pins 25 & 26 or Left and Right
 */

#include <ESPdLib.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    ESPdLib::Config config;
    config.bclkPin = 38;
    config.wsPin = 39;
    config.doutPin = 40;
    // config.useInternalDAC = true;   // Uncomment to route output to GPIO25 (L) / GPIO26 (R) on OG ESP32

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
    delay(1000); // do nothing, audio processesing is done in the background
}

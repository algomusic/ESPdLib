/*
 * ASineTone_DAC - ESPdLib Internal DAC Example
 *
 * Plays a 440 Hz sine wave through the ESP32's internal DAC (8-bit).
 * Upload the data/ folder to LittleFS first:
 *   Cmd+Shift+P -> "Upload LittleFS to Pico/ESP8266/ESP32"
 *   Ensure that no Arduino sketch has the Serial Monitor open
 *   Older ESP32s may require a slower upload speed
 *
 * Hardware:
 *   - ESP32 (GPIO25 = left, GPIO26 = right) or
 *     ESP32-S2 (GPIO17 = left, GPIO18 = right)
 *   - No external DAC needed — connect headphones/amp directly to DAC pins
 *   - Not available on ESP32-S3, C3, or other chips without internal DAC
 *
 * Note: Internal DAC is 8-bit, so audio quality is lower than an external
 * I2S DAC. Fine for oscillators and simple patches; less ideal for
 * wide-dynamic-range effects.
 */

#include <ESPdLib.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    ESPdLib::Config config;
    config.useInternalDAC = true;
    // I2S pin settings are ignored when useInternalDAC is true.
    // Sample rate can still be adjusted (default 48000).

    if (!Pd.begin(config)) {
        Serial.println("ESPdLib init failed!");
        while (1) delay(1000);
    }

    if (!Pd.openPatch("sine-tone.pd")) {
        Serial.println("Failed to open patch!");
        while (1) delay(1000);
    }

    Serial.println("Playing 440 Hz sine tone via internal DAC");
}

void loop() {
    delay(1000); // do nothing, audio processesing is done in the background
}

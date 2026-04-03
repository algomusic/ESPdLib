/*
 * EmbeddedPatch - ESPdLib Example
 *
 * Embeds a Pd patch as a string in the sketch and writes it to LittleFS
 * at runtime, so no separate data/ upload step is needed.
 *
 * Hardware:
 *   - ESP32 board
 *   - I2S DAC (e.g., MAX98357A or PCM5102) on pins BCLK=38, WS=39, DOUT=40
 */

#include <ESPdLib.h>
#include <LittleFS.h>

// Pd patch embedded as a string: [osc~ 440] -> [dac~]
static const char PATCH[] =
    "#N canvas 597 423 450 300 12;\n"
    "#X obj 147 121 osc~ 440;\n"
    "#X obj 147 145 dac~;\n"
    "#X connect 0 0 1 0;\n"
    "#X connect 0 0 1 1;\n";
static const char PATCH_NAME[] = "sine-tone.pd";

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

    // Write the embedded patch to LittleFS
    File f = LittleFS.open(String("/") + PATCH_NAME, "w");
    if (f) {
        f.print(PATCH);
        f.close();
    }

    if (!Pd.openPatch(PATCH_NAME)) {
        Serial.println("Failed to open patch!");
        while (1) delay(1000);
    }

    Serial.println("Playing embedded patch");
}

void loop() {
    delay(1000);
}

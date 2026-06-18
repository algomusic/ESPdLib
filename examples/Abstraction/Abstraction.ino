/*
 * Abstraction - ESPdLib Pd Abstraction Example
 *
 * Demonstrates that .pd abstractions now work with ESPdLib.
 *
 * The parent patch (abstraction-demo.pd) drives an [osc~ 440] through a
 * [square] object. [square] is NOT a built-in Pd object — it is loaded
 * from the abstraction file square.pd, which clips the oscillator into a
 * square-ish waveform. This is the same DSP as the SubPatch_DAC example,
 * but factored out into a reusable abstraction file instead of an inline
 * subpatch.
 *
 * IMPORTANT: both .pd files must be uploaded to LittleFS. The abstraction
 * (square.pd) has to live in the same directory as the patch that uses it
 * (or on a directory added to the Pd search path), otherwise [square]
 * fails to instantiate and you'll get a "couldn't create" message.
 *
 * Upload the data/ folder to LittleFS first:
 *   Cmd+Shift+P -> "Upload LittleFS to Pico/ESP8266/ESP32"
 *   Ensure that no Arduino sketch has the Serial Monitor open
 *   Older ESP32s may require a slower upload speed
 *
 * Hardware:
 *   - ESP32 board
 *   - I2S DAC (e.g., MAX98357A or PCM5102) on pins BCLK=38, WS=39, DOUT=40
 *   - or internal DAC on original ESP32 (set config.useInternalDAC = true)
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

    if (!Pd.openPatch("abstraction-demo.pd")) {
        Serial.println("Failed to open patch!");
        Serial.println("Did you upload BOTH abstraction-demo.pd and square.pd to LittleFS?");
        while (1) delay(1000);
    }

    Serial.println("Playing 440 Hz tone shaped by the [square] abstraction");
}

void loop() {
    delay(1000); // do nothing, audio processing is done in the background
}

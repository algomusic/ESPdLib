/*
 * SimplePhasor - ESPdLib Example
 *
 * Plays a phasor-based triangle wave through a low-pass filter.
 * Three potentiometers control frequency, amplitude, and filter cutoff.
 *
 * Patch signal flow:
 *   [r freq] -> [phasor~] -> [*~ 2] -> [-~ 1] -> [lop~ cutoff] -> [*~ amp] -> [dac~]
 *
 * Hardware:
 *   - ESP32 board
 *   - I2S DAC (e.g., MAX98357A or PCM5102) on pins BCLK=38, WS=39, DOUT=40
 *   - Potentiometer on GPIO1 (frequency: 50-2000 Hz)
 *   - Potentiometer on GPIO2 (amplitude: 0-1)
 *   - Potentiometer on GPIO3 (cutoff: 50-10000 Hz)
 *
 * Upload the data/ folder to LittleFS first:
 *   Cmd+Shift+P -> "Upload LittleFS to Pico/ESP8266/ESP32"
 */

#include <ESPdLib.h>

#define POT_FREQ_PIN   1
#define POT_AMP_PIN    2
#define POT_CUTOFF_PIN 3

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("SimplePhasor - ESPdLib Example");

    ESPdLib::Config config;
    config.bclkPin = 38;
    config.wsPin = 39;
    config.doutPin = 40;

    if (!Pd.begin(config)) {
        Serial.println("ESPdLib init failed!");
        while (1) delay(1000);
    }

    Pd.onPrint([](const char* msg) { Serial.printf("[Pd] %s", msg); });

    if (!Pd.openPatch("simple-phasor.pd")) {
        Serial.println("Failed to open patch!");
        while (1) delay(1000);
    }

    Pd.sendFloat("freq", 220.0);
    Pd.sendFloat("amp", 0.3);
    Pd.sendFloat("cutoff", 5000.0);

    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
    float freq = 50.0 + (analogRead(POT_FREQ_PIN) / 4095.0) * 1950.0;
    float amp = analogRead(POT_AMP_PIN) / 4095.0;
    float cutoff = 50.0 + pow(analogRead(POT_CUTOFF_PIN) / 4095.0, 2) * 10000.0;

    Pd.sendFloat("freq", freq);
    Pd.sendFloat("amp", amp);
    Pd.sendFloat("cutoff", cutoff);

    delay(20);
}

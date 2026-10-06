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
 *   - I2S DAC (e.g., MAX98357A or PCM5102)
 *   - Optional ES8311 DAC, ES7210 microphones and TCA9555 amplifier, used on
 *     the Waveshare ESP32-S3-AUDIO-Board
 *   - Potentiometer on GPIO1 (frequency: 50-2000 Hz)
 *   - Potentiometer on GPIO2 (amplitude: 0-1)
 *
 * Upload the data/ folder to LittleFS first:
 *   Cmd+Shift+P -> "Upload LittleFS to Pico/ESP8266/ESP32"
 */

#include <ESPdLib.h>
#include <ESPdLibCodecs.h>

constexpr bool USE_WAVESHARE_PINOUT = false;
constexpr bool USE_ES8311_CODEC = false;
constexpr bool USE_ES7210_MIC = false;
constexpr bool USE_TCA9555_AMP = false;

constexpr int I2S_BCLK_PIN = USE_WAVESHARE_PINOUT ? 13 : 38;
constexpr int I2S_WS_PIN = USE_WAVESHARE_PINOUT ? 14 : 39;
constexpr int I2S_DOUT_PIN = USE_WAVESHARE_PINOUT ? 16 : 40;
constexpr int I2S_DIN_PIN = USE_WAVESHARE_PINOUT ? 15 : -1;
constexpr int I2S_MCLK_PIN = USE_WAVESHARE_PINOUT ? 12 : -1;
constexpr int AUDIO_SAMPLE_RATE = (USE_ES8311_CODEC || USE_ES7210_MIC) ? 44100 : 48000;

#define POT_FREQ_PIN 1
#define POT_AMP_PIN  2

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("SimpleSinewave - ESPdLib Example");

    ESPdLib::Config config;
    config.bclkPin = I2S_BCLK_PIN;
    config.wsPin = I2S_WS_PIN;
    config.doutPin = I2S_DOUT_PIN;
    config.dinPin = I2S_DIN_PIN;
    config.mclkPin = I2S_MCLK_PIN;
    config.sampleRate = AUDIO_SAMPLE_RATE;
    config.useES8311Codec = USE_ES8311_CODEC;
    config.useES7210Mic = USE_ES7210_MIC;
    config.useTCA9555Amp = USE_TCA9555_AMP;
    config.i2cSclPin = 10;
    config.i2cSdaPin = 11;
    config.es8311VolumeDb = -9;

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

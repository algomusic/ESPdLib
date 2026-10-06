/*
 * EmbeddedPatch - ESPdLib Example
 *
 * Embeds a Pd patch as a string in the sketch and writes it to LittleFS
 * at runtime, so no separate data/ upload step is needed.
 *
 * Hardware:
 *   - By default: ESP32 with a no-setup I2S DAC like the PCM5102 or MAX9837 on pins BCLK=38, WS=39, DOUT=40
 *   - Optional ES8311 DAC, ES7210 microphones, and TCA9555 amplifier that require setup over i2c. These devices 
 *     are present on the Waveshare ESP32-S3-AUDIO-Board.
 */

#include <ESPdLib.h>
#include <ESPdLibCodecs.h>
#include <LittleFS.h>

// Pd patch embedded as a string: [osc~ 440] -> [dac~]
static const char PATCH[] =
    "#N canvas 597 423 450 300 12;\n"
    "#X obj 147 121 osc~ 440;\n"
    "#X obj 147 145 dac~;\n"
    "#X connect 0 0 1 0;\n"
    "#X connect 0 0 1 1;\n";
static const char PATCH_NAME[] = "sine-tone.pd";

// Select the board pin map separately from the I2C audio devices.
// These pin assignments match the Waveshare ESP32-S3-AUDIO-Board.
constexpr bool USE_WAVESHARE_PINOUT = false;
// The Waveshare ESP32-S3-AUDIO-Board includes all three devices below.
constexpr bool USE_ES8311_CODEC = false;
constexpr bool USE_ES7210_MIC = false;
constexpr bool USE_TCA9555_AMP = false;

constexpr int I2S_BCLK_PIN = USE_WAVESHARE_PINOUT ? 13 : 38;
constexpr int I2S_WS_PIN = USE_WAVESHARE_PINOUT ? 14 : 39;
constexpr int I2S_DOUT_PIN = USE_WAVESHARE_PINOUT ? 16 : 40; // ESP32 -> codec DIN
constexpr int I2S_DIN_PIN = USE_WAVESHARE_PINOUT ? 15 : -1; // codec DOUT -> ESP32
constexpr int I2S_MCLK_PIN = USE_WAVESHARE_PINOUT ? 12 : -1;
constexpr int AUDIO_SAMPLE_RATE = (USE_ES8311_CODEC || USE_ES7210_MIC) ? 44100 : 48000;

void setup() {
    Serial.begin(115200);
    delay(1000);

    ESPdLib::Config config;
    config.bclkPin = I2S_BCLK_PIN;
    config.wsPin = I2S_WS_PIN;
    config.doutPin = I2S_DOUT_PIN; // ESP32 I2S DOUT -> codec DIN
    config.dinPin = I2S_DIN_PIN;
    config.mclkPin = I2S_MCLK_PIN;
    config.sampleRate = AUDIO_SAMPLE_RATE;

    // I2C device support is optional and independent of the selected pin map.
    config.i2cSclPin = 10;
    config.i2cSdaPin = 11;
    config.useES8311Codec = USE_ES8311_CODEC;
    config.useES7210Mic = USE_ES7210_MIC;
    config.useTCA9555Amp = USE_TCA9555_AMP;
    config.es8311VolumeDb = -9; // hardware DAC volume in dB; safe initial level

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

    // Optional runtime hardware controls are available, for example:
    // ESPdLibCodecs::es8311SetVolumeDb(-12);
    // ESPdLibCodecs::es7210SetMicGainDb(24);
    Serial.println("Playing embedded patch");
}

void loop() {
    delay(1000);
}

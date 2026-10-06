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
 *   - I2S DAC (e.g., MAX98357A or PCM5102)
 *   - Optional ES8311 DAC, ES7210 microphones and TCA9555 amplifier, used on
 *     the Waveshare ESP32-S3-AUDIO-Board
 *   - or internal DAC on original ESP32 (set config.useInternalDAC = true)
 */

#include <ESPdLib.h>
#include <ESPdLibCodecs.h>

// openPatch() loads abstractions on the Arduino loopTask, and libpd's
// binbuf_evalfile recurses one level per nesting depth. The default 8 KB
// loopTask stack can overflow with nested/multiple abstractions, crashing
// inside openPatch(). Raise it here (must be at file scope, not in setup()).
SET_LOOP_TASK_STACK_SIZE(64 * 1024);

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

void setup() {
    Serial.begin(115200);
    delay(1000);

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

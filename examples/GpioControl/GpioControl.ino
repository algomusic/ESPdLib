/*
 * GpioControl - ESPdLib Example
 *
 * Demonstrates bidirectional communication between ESP32 GPIO and a
 * Pure Data patch. Multiple analog inputs control synthesis parameters,
 * and a digital button triggers events in the patch.
 *
 * The patch has two oscillators (main + detuned) with amplitude control:
 *   [r freq]   -> [osc~] -----> [*~ amp] -> [+~] -> [clip~] -> [dac~]
 *   [r detune] -> [osc~ * 0.3] ----------^
 *   [r trigger] -> sends bang to [s level]
 *
 * Hardware:
 *   - ESP32-S3 board
 *   - I2S DAC on pins BCLK=38, WS=39, DOUT=40
 *   - Optional ES8311 DAC, ES7210 microphones and TCA9555 amplifier, used on
 *     the Waveshare ESP32-S3-AUDIO-Board
 *   - Potentiometer on GPIO1 (frequency: 50-1000 Hz)
 *   - Potentiometer on GPIO2 (amplitude: 0-1)
 *   - Potentiometer on GPIO3 (detune frequency: 50-1000 Hz)
 *   - Button on GPIO4 (trigger, active low with pullup)
 *   - LED on GPIO8 (lights on trigger feedback from Pd)
 *
 * Upload the data/ folder to LittleFS first.
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

#define POT_FREQ_PIN    1
#define POT_AMP_PIN     2
#define POT_DETUNE_PIN  3
#define BUTTON_PIN      20
#define LED_PIN         47

void* patch = nullptr;
bool lastButton = false;

void onPdPrint(const char* msg) {
    Serial.printf("[Pd] %s", msg);
}

void onPdFloat(const char* source, float value) {
    Serial.printf("[Pd] %s = %.2f\n", source, value);
    // When Pd sends to "level", flash the LED
    if (strcmp(source, "level") == 0) {
        digitalWrite(LED_PIN, HIGH);
        // LED will be turned off in loop()
    } else digitalWrite(LED_PIN, LOW);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("GpioControl - ESPdLib Example");

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    ESPdLib::Config config;
    config.sampleRate = AUDIO_SAMPLE_RATE;
    config.bclkPin = I2S_BCLK_PIN;
    config.wsPin = I2S_WS_PIN;
    config.doutPin = I2S_DOUT_PIN;
    config.dinPin = I2S_DIN_PIN;
    config.mclkPin = I2S_MCLK_PIN;
    config.useES8311Codec = USE_ES8311_CODEC;
    config.useES7210Mic = USE_ES7210_MIC;
    config.useTCA9555Amp = USE_TCA9555_AMP;
    config.i2cSclPin = 10;
    config.i2cSdaPin = 11;
    config.es8311VolumeDb = -9;

    if (!Pd.begin(config)) {
        Serial.println("ERROR: ESPdLib init failed!");
        while (1) delay(1000);
    }

    Pd.onPrint(onPdPrint);
    Pd.onFloat(onPdFloat);

    // Subscribe to receive "level" messages from Pd
    Pd.subscribe("level");

    patch = Pd.openPatch("gpio-patch.pd");
    if (!patch) {
        Serial.println("ERROR: Failed to open patch!");
        while (1) delay(1000);
    }

    // Set initial values
    Pd.sendFloat("freq", 220.0);
    Pd.sendFloat("amp", 0.3);
    Pd.sendFloat("detune", 223.0);

    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
    // Read analog inputs
    float freq = 50.0 + (analogRead(POT_FREQ_PIN) / 4095.0) * 950.0;
    float amp = analogRead(POT_AMP_PIN) / 4095.0;
    float detune = 50.0 + (analogRead(POT_DETUNE_PIN) / 4095.0) * 950.0;

    Pd.sendFloat("freq", freq);
    Pd.sendFloat("amp", amp);
    Pd.sendFloat("detune", detune);

    // Button trigger (active low, edge detection)
    bool button = !digitalRead(BUTTON_PIN);
    if (button && !lastButton) {
        Pd.sendFloat("trigger", button);
        Serial.println("Button pressed -> trigger");
    }
    lastButton = button;

    // Turn off LED after brief flash
    static unsigned long ledOnTime = 0;
    if (digitalRead(LED_PIN) && millis() - ledOnTime > 50) {
        digitalWrite(LED_PIN, LOW);
    }
    if (digitalRead(LED_PIN) && ledOnTime == 0) {
        ledOnTime = millis();
    }
    if (!digitalRead(LED_PIN)) {
        ledOnTime = 0;
    }

    Pd.processSerial();
    delay(10);
}

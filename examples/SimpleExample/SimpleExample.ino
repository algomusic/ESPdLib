/*
 * SimpleExample - ESPdLib Example
 *
 * Loads a Pure Data patch and plays audio through I2S. Potentiometers on
 * GPIO1, GPIO2 and GPIO3 control frequency (100-2000 Hz), amplitude (0-1), cutoff (50 - 1000 Hz)
 * via Pd [receive] objects named "freq" and "amp".
 *
 * ---- Loading patches ----
 *
 * Patches are stored on the ESP32's LittleFS flash filesystem.
 * There are three ways to get .pd files onto LittleFS:
 *
 *   1. LittleFS Upload Tool (recommended):
 *      - Place .pd files in this sketch's data/ folder
 *      - In Arduino IDE: Cmd+Shift+P -> "Upload LittleFS to Pico/ESP8266/ESP32"
 *      - Close Serial Monitor first or the upload will fail!
 *      - See: https://github.com/earlephilhower/arduino-littlefs-upload
 *
 *   2. Serial upload (no recompile needed):
 *      - python3 scripts/upload_patch.py /dev/cu.usbmodem* my-patch.pd
 *      - Uses the PATCH_BEGIN/PATCH_END protocol via Pd.processSerial()
 *
 *   3. Fallback: A default "simple-sinewave.pd" patch is embedded in this
 *      sketch and auto-written to LittleFS if no .pd files are found.
 *
 * Change PATCH_TO_LOAD below to select the initial patch to open.
 *
 * ---- Switching patches at runtime ----
 *
 * Type a patch number into the Arduino Serial Monitor and press Enter
 * to hot-swap to that patch. The available patches and their numbers
 * are listed at startup and after each switch.
 *
 * ---- Controlling patches from code ----
 *
 * Use Pd.sendFloat("name", value) to send values to [receive name] objects
 * in your patch. The patch must use [r freq] and [r amp] for the pot
 * controls in this example to work.
 *
 * ---- Hardware ----
 *
 *   - ESP32-S3 board
 *   - I2S DAC (e.g., MAX98357A or PCM5102) on pins BCLK=38, WS=39, DOUT=40
 *   - Potentiometer on GPIO1 (frequency)
 *   - Potentiometer on GPIO2 (amplitude)
 */

#include <ESPdLib.h>
#include <LittleFS.h>

// ---- Configuration ----

// Which patch to load from LittleFS (must match a filename in data/)
#define PATCH_TO_LOAD  "simple-phasor.pd"

// Potentiometer pins (set to -1 to disable)
#define POT_FREQ_PIN 1
#define POT_AMP_PIN  2
#define POT_CUTOFF_PIN  3

// ---- Default fallback patch ----
// Embedded in the sketch so there's always something to play, even before
// any .pd files have been uploaded to LittleFS.
// Patch: [r freq] -> [osc~] -> [*~] -> [dac~], with [r amp] controlling gain.
static const char DEFAULT_PATCH[] =
    "#N canvas 64 96 450 300 12;\n"
    "#X obj 145 30 r freq;\n"
    "#X obj 145 55 osc~;\n"
    "#X obj 145 80 *~;\n"
    "#X obj 145 105 dac~;\n"
    "#X obj 200 55 r amp;\n"
    "#X connect 0 0 1 0;\n"
    "#X connect 1 0 2 0;\n"
    "#X connect 2 0 3 0;\n"
    "#X connect 2 0 3 1;\n"
    "#X connect 4 0 2 1;\n";
static const char DEFAULT_PATCH_NAME[] = "simple-sinewave.pd";

// ---- Globals ----

void* patch = nullptr;

// Patch list for runtime switching via Serial Monitor
#define MAX_PATCHES 16
String patchNames[MAX_PATCHES];
int numPatches = 0;

void onPdPrint(const char* msg) {
    Serial.printf("[Pd] %s", msg);
}

// Scan LittleFS and populate the patchNames[] array.
// Also prints the numbered list to Serial.
void buildPatchList() {
    numPatches = 0;
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) return;
    File f = root.openNextFile();
    while (f && numPatches < MAX_PATCHES) {
        String name = f.name();
        if (name.endsWith(".pd")) {
            patchNames[numPatches] = name;
            numPatches++;
        }
        f = root.openNextFile();
    }
    // Print the list
    Serial.println("Available patches on LittleFS:");
    for (int i = 0; i < numPatches; i++) {
        Serial.printf("  [%d] %s\n", i + 1, patchNames[i].c_str());
    }
    if (numPatches == 0) {
        Serial.println("  (none)");
    }
    Serial.println("Type a number + Enter in Serial Monitor to switch.\n");
}

// Switch to a patch by index (1-based). Closes the current patch first.
void switchToPatch(int num) {
    if (num < 1 || num > numPatches) {
        Serial.printf("Invalid patch number %d (range: 1-%d)\n\n", num, numPatches);
        return;
    }
    const char* name = patchNames[num - 1].c_str();
    Serial.printf("Switching to patch [%d] '%s'...\n", num, name);

    // Close current patch
    if (patch) {
        Pd.closePatch(patch);
        patch = nullptr;
    }

    // Open new patch
    patch = Pd.openPatch(name);
    if (!patch) {
        Serial.printf("ERROR: Failed to open '%s'!\n\n", name);
        return;
    }
    Serial.printf("Now playing: %s\n", name);
    Serial.printf("Free heap: %d bytes\n\n", ESP.getFreeHeap());
}

// Write the default fallback patch to LittleFS if no .pd files exist at all.
// This ensures the board always has something to play out of the box.
void ensureFallbackPatch() {
    // Check if any .pd files already exist on LittleFS
    File root = LittleFS.open("/");
    if (root && root.isDirectory()) {
        File f = root.openNextFile();
        while (f) {
            String name = f.name();
            if (name.endsWith(".pd")) {
                // At least one .pd file exists — no fallback needed
                return;
            }
            f = root.openNextFile();
        }
    }

    // No .pd files found — write the default patch
    Serial.println("No patches found on LittleFS — writing default fallback patch");
    File f = LittleFS.open(String("/") + DEFAULT_PATCH_NAME, "w");
    if (f) {
        f.print(DEFAULT_PATCH);
        f.close();
        Serial.printf("  Wrote '%s' (%d bytes)\n", DEFAULT_PATCH_NAME, strlen(DEFAULT_PATCH));
    } else {
        Serial.printf("  ERROR: failed to write '%s'\n", DEFAULT_PATCH_NAME);
    }
}

float prevFreq = 440.0;
unsigned long lastCpuCheck = millis();

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================");
    Serial.println("  ESPdLib - SimpleExample Example");
    Serial.println("========================================\n");

    // Configure ESPdLib
    ESPdLib::Config config;
    config.sampleRate = 48000;
    config.numOutputChannels = 2;
    config.numInputChannels = 0;
    // ESP32-S3 I2S pins — adjust for your board
    config.bclkPin = 38;
    config.wsPin = 39;
    config.doutPin = 40;

    // Initialize ESPdLib (mounts LittleFS, starts I2S, starts audio task)
    Serial.println("Initializing ESPdLib...");
    if (!Pd.begin(config)) {
        Serial.println("ERROR: ESPdLib init failed! Check I2S wiring.");
        while (1) delay(1000);
    }
    Serial.println("ESPdLib initialized OK\n");

    // Write fallback patch if LittleFS is empty
    ensureFallbackPatch();

    // Build the numbered patch list for Serial Monitor switching
    buildPatchList();

    // Set print callback to see Pd console output
    Pd.onPrint(onPdPrint);

    // Open the initial patch
    Serial.printf("Loading patch '%s'...\n", PATCH_TO_LOAD);
    if (!Pd.patchExists(PATCH_TO_LOAD)) {
        Serial.printf("WARNING: '%s' not found on LittleFS!\n", PATCH_TO_LOAD);
        Serial.println("Upload patches using the LittleFS upload tool:");
        Serial.println("  1. Put .pd files in the sketch's data/ folder");
        Serial.println("  2. Cmd+Shift+P -> 'Upload LittleFS to Pico/ESP8266/ESP32'");
        Serial.println("Falling back to default patch...\n");
        patch = Pd.openPatch(DEFAULT_PATCH_NAME);
    } else {
        patch = Pd.openPatch(PATCH_TO_LOAD);
    }

    if (!patch) {
        Serial.println("ERROR: Failed to open patch!");
        while (1) delay(1000);
    }

    // Set initial values for [r freq] and [r amp] receivers in the patch
    Pd.sendFloat("freq", 440.0);
    Pd.sendFloat("amp", 0.3);
    Pd.sendFloat("cutoff", 5000);

    Serial.printf("Now playing: %s\n", PATCH_TO_LOAD);
    Serial.println("Adjust pots to change freq/amp/cutoff.");
    Serial.printf("Free heap: %d bytes\n\n", ESP.getFreeHeap());
}

void loop() {
    // Read potentiometers and map to frequency and amplitude
    int rawFreq = analogRead(POT_FREQ_PIN);
    int rawAmp = analogRead(POT_AMP_PIN);
    int rawCutoff = analogRead(POT_CUTOFF_PIN);

    // Map ADC (0-4095) to frequency (100-2000 Hz) and amplitude (0-1)
    float freq = (prevFreq * 9 + (100.0 + (rawFreq / 4095.0) * 1900.0)) / 10.0;
    prevFreq = freq;
    float amp = rawAmp / 4095.0;
    float cutoff = 50 + pow(rawCutoff / 4095.0, 2) * 10000.0;
    // Serial.println("amp " + String(amp) +  " cut " + String(cutoff));

    // Send values to Pd [r freq] and [r amp] receivers
    Pd.sendFloat("freq", freq);
    Pd.sendFloat("amp", amp);
    Pd.sendFloat("cutoff", cutoff);

    // Handle serial input: number to switch patch, or PATCH_BEGIN for upload
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();

        if (line.startsWith("PATCH_BEGIN")) {
            // Hand off to ESPdLib's serial upload protocol
            // Push the line back by processing it manually
            Pd.processSerial();
        } else if (line.length() > 0) {
            int num = line.toInt();
            if (num > 0) {
                switchToPatch(num);
                // Re-send initial values so the new patch starts with sensible defaults
                Pd.sendFloat("freq", 440.0);
                Pd.sendFloat("amp", 0.3);
                Pd.sendFloat("cutoff", 5000.0);
            } else {
                // Unknown command — show help
                Serial.println("Commands: type a patch number (1, 2, ...) to switch patches\n");
                buildPatchList();
            }
        }
    }

    if (millis() - lastCpuCheck > 10000) {
        lastCpuCheck = millis();
        Serial.println("CPU load " + String(Pd.getCpuLoad()));
    }

    delay(20);  // ~50 Hz control rate
}

// ESPdLib - A lightweight Pure Data libpd wrapper for ESP32
// Compiling requires Arduino-ESP32 core v3.x
// Provides thread-safe message sending, patch management, and audio processing.
// Uses LittleFS for patch storage and supports serial patch upload.
// Inspired by Miller Puckette's original libpd and espd.

#ifndef ESPDLIB_H
#define ESPDLIB_H

#include <Arduino.h>

// Callback types for receiving messages from Pd patches
typedef void (*PdFloatCallback)(const char* source, float value);
typedef void (*PdBangCallback)(const char* source);
typedef void (*PdPrintCallback)(const char* message);

struct ESPdLibConfig {
    int sampleRate        = 48000;
    int numOutputChannels = 2;
    int numInputChannels  = 0;
    // ESP32-S3 default I2S pins (adjust for your board)
    int bclkPin           = 7;
    int wsPin             = 6;
    int doutPin           = 5;
    int dinPin            = -1;  // -1 = no audio input
    // FreeRTOS audio task settings
    // Defaults to core 1 on dual-core chips, core 0 on single-core (ESP32-S2, C3)
    int audioTaskCore     = (portNUM_PROCESSORS > 1) ? 1 : 0;
    int audioTaskPriority = 20;
    int audioTaskStack    = 8192;
    // PSRAM settings (requires PSRAM enabled in Arduino IDE: Tools > PSRAM)
    // When enabled, malloc() allocations larger than psramMinAllocSize are
    // placed in PSRAM, freeing internal SRAM for small, latency-sensitive data.
    // Useful for patches with large [table]/[array] objects, delay lines, etc.
    bool usePSRAM         = false;
    int  psramMinAllocSize = 512;  // Allocations >= this size go to PSRAM (bytes)
};

class ESPdLib {
public:
    typedef ESPdLibConfig Config;

    ESPdLib();
    ~ESPdLib();

    // Initialize libpd, mount LittleFS, configure I2S, and start the audio task.
    // Returns true on success.
    bool begin(const Config& config);

    // Stop audio task and release resources.
    void end();

    // --- Patch Management ---

    // Open a .pd patch file from the filesystem (LittleFS by default).
    // Returns an opaque handle, or NULL on failure.
    void* openPatch(const char* filename, const char* dir = "/littlefs");

    // Close a previously opened patch.
    void closePatch(void* patch);

    // --- Send Messages TO Pd (thread-safe, queued) ---
    // These can be called safely from loop() or any thread.

    // Send a float to a Pd [receive <receiver>] object.
    void sendFloat(const char* receiver, float value);

    // Send a bang to a Pd [receive <receiver>] object.
    void sendBang(const char* receiver);

    // Send a symbol to a Pd [receive <receiver>] object.
    void sendSymbol(const char* receiver, const char* symbol);

    // --- Receive Messages FROM Pd (callbacks) ---
    // Callbacks fire from the audio task thread.

    // Set callback for float messages from Pd [send <source>] objects.
    void onFloat(PdFloatCallback callback);

    // Set callback for bang messages from Pd [send <source>] objects.
    void onBang(PdBangCallback callback);

    // Set callback for [print] output from Pd.
    void onPrint(PdPrintCallback callback);

    // Subscribe to receive messages from a named Pd [send] source.
    // Must be called before messages from that source will trigger callbacks.
    void subscribe(const char* source);

    // Unsubscribe from a named source.
    void unsubscribe(const char* source);

    // --- Array/Table Access ---
    // These access Pd arrays (tables) directly. Call from the audio task
    // context or ensure the audio task is not running.

    int readArray(float* dest, const char* name, int offset, int n);
    int writeArray(const char* name, int offset, const float* src, int n);

    // --- Status ---

    bool isRunning() const;
    float getCpuLoad() const;

    // --- Filesystem Helpers ---

    // Print all .pd files found on LittleFS to Serial.
    // Returns the number of patches found.
    int listPatches();

    // Check if a .pd file exists on LittleFS.
    bool patchExists(const char* filename);

    // --- Serial Patch Upload ---
    // Call from loop() to process incoming serial data for patch upload.
    // Protocol: "PATCH_BEGIN filename\n" ... patch text ... "PATCH_END\n"
    void processSerial();

private:
    static void audioTaskFunc(void* param);
    void drainMessageQueue();

    struct ESPdLibImpl* _impl;
};

// Global singleton
extern ESPdLib Pd;

#endif // ESPDLIB_H

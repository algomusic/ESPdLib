#include "ESPdLib.h"
#include "pd_audio.h"
#include "pd_message_queue.h"
#include "pd_libpd_wrapper.h"

#include <LittleFS.h>
#include "esp_heap_caps.h"

#define PD_BLOCK_SIZE 64
#define PD_MAX_SUBSCRIPTIONS 32

// --- Internal state ---

struct ESPdLibImpl {
    ESPdLibConfig config;
    TaskHandle_t audioTask = NULL;
    volatile bool running = false;
    PdMessageQueue msgQueue;

    // Callbacks
    PdFloatCallback floatCb = nullptr;
    PdBangCallback bangCb = nullptr;
    PdPrintCallback printCb = nullptr;

    // CPU load tracking
    volatile float cpuLoad = 0.0f;

    // Subscription tracking (pdw_bind returns void* handles)
    struct Sub { const char* name; void* handle; };
    Sub subs[PD_MAX_SUBSCRIPTIONS];
    int numSubs = 0;

    // Serial upload state
    bool serialUploading = false;
    String serialFilename;
    String serialBuffer;
};

// Global singleton
ESPdLib Pd;

// Singleton pointer for static hooks
static ESPdLibImpl* s_impl = nullptr;

// --- libpd hooks (called from audio task context) ---

static void pd_print_hook(const char* s) {
    if (s_impl && s_impl->printCb) {
        s_impl->printCb(s);
    }
}

static void pd_float_hook(const char* recv, float x) {
    if (s_impl && s_impl->floatCb) {
        s_impl->floatCb(recv, x);
    }
}

static void pd_bang_hook(const char* recv) {
    if (s_impl && s_impl->bangCb) {
        s_impl->bangCb(recv);
    }
}

// --- ESPdLib implementation ---

ESPdLib::ESPdLib() : _impl(nullptr) {}

ESPdLib::~ESPdLib() {
    end();
}

bool ESPdLib::begin(const Config& config) {
    if (_impl) return false; // already initialized

    _impl = new ESPdLibImpl();
    _impl->config = config;
    s_impl = _impl;

    // Configure PSRAM if requested and available
    if (config.usePSRAM) {
        if (psramFound()) {
            heap_caps_malloc_extmem_enable(config.psramMinAllocSize);
            Serial.printf("ESPdLib: PSRAM enabled (%d KB available, threshold >= %d bytes)\n",
                          ESP.getFreePsram() / 1024, config.psramMinAllocSize);
        } else {
            Serial.println("ESPdLib: WARNING - usePSRAM requested but no PSRAM detected");
        }
    }

    // Mount LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("ESPdLib: LittleFS mount failed");
        delete _impl;
        _impl = nullptr;
        s_impl = nullptr;
        return false;
    }

    // Initialize libpd
    pdw_set_printhook(pd_print_hook);
    pdw_set_banghook(pd_bang_hook);
    pdw_set_floathook(pd_float_hook);

    if (pdw_init()) {
        Serial.println("ESPdLib: pdw_init failed");
        delete _impl;
        _impl = nullptr;
        s_impl = nullptr;
        return false;
    }

    // Configure audio: input channels, output channels, sample rate
    if (pdw_init_audio(config.numInputChannels, config.numOutputChannels, config.sampleRate)) {
        Serial.println("ESPdLib: pdw_init_audio failed");
        delete _impl;
        _impl = nullptr;
        s_impl = nullptr;
        return false;
    }

    // Add LittleFS root to Pd search path
    pdw_add_to_search_path("/littlefs");

    // Turn on DSP: send [; pd dsp 1(
    pdw_start_message(1);
    pdw_add_float(1);
    pdw_finish_message("pd", "dsp");

    // Initialize audio output
    if (config.useInternalDAC) {
        if (!pd_audio_init_dac(config.sampleRate, config.numOutputChannels)) {
            Serial.println("ESPdLib: internal DAC init failed (unsupported chip?)");
            delete _impl;
            _impl = nullptr;
            s_impl = nullptr;
            return false;
        }
        Serial.println("ESPdLib: using internal DAC (8-bit output)");
    } else {
        if (!pd_audio_init(config.sampleRate, config.numOutputChannels, config.numInputChannels,
                           config.bclkPin, config.wsPin, config.doutPin, config.dinPin)) {
            Serial.println("ESPdLib: I2S init failed");
            delete _impl;
            _impl = nullptr;
            s_impl = nullptr;
            return false;
        }
    }

    // Start audio processing task
    _impl->running = true;
    xTaskCreatePinnedToCore(
        audioTaskFunc,
        "pd_audio",
        config.audioTaskStack,
        this,
        config.audioTaskPriority,
        &_impl->audioTask,
        config.audioTaskCore
    );

    Serial.println("ESPdLib: initialized");
    Serial.printf("ESPdLib: free internal RAM: %d KB", ESP.getFreeHeap() / 1024);
    if (psramFound()) {
        Serial.printf(", free PSRAM: %d KB", ESP.getFreePsram() / 1024);
    }
    Serial.println();
    return true;
}

void ESPdLib::end() {
    if (!_impl) return;

    _impl->running = false;
    if (_impl->audioTask) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        _impl->audioTask = NULL;
    }

    if (_impl->config.useInternalDAC) {
        pd_audio_deinit_dac();
    } else {
        pd_audio_deinit();
    }
    LittleFS.end();

    s_impl = nullptr;
    delete _impl;
    _impl = nullptr;
}

void* ESPdLib::openPatch(const char* filename, const char* dir) {
    if (!_impl) return nullptr;
    void* patch = pdw_openfile(filename, dir);
    if (!patch) {
        Serial.printf("ESPdLib: failed to open patch %s/%s\n", dir, filename);
    }
    return patch;
}

void ESPdLib::closePatch(void* patch) {
    if (patch) {
        pdw_closefile(patch);
    }
}

// --- Thread-safe message sending ---

void ESPdLib::sendFloat(const char* receiver, float value) {
    if (!_impl) return;
    PdMessage msg;
    msg.type = PD_MSG_FLOAT;
    strncpy(msg.receiver, receiver, sizeof(msg.receiver) - 1);
    msg.receiver[sizeof(msg.receiver) - 1] = '\0';
    msg.floatVal = value;
    _impl->msgQueue.push(msg);
}

void ESPdLib::sendBang(const char* receiver) {
    if (!_impl) return;
    PdMessage msg;
    msg.type = PD_MSG_BANG;
    strncpy(msg.receiver, receiver, sizeof(msg.receiver) - 1);
    msg.receiver[sizeof(msg.receiver) - 1] = '\0';
    _impl->msgQueue.push(msg);
}

void ESPdLib::sendSymbol(const char* receiver, const char* symbol) {
    if (!_impl) return;
    PdMessage msg;
    msg.type = PD_MSG_SYMBOL;
    strncpy(msg.receiver, receiver, sizeof(msg.receiver) - 1);
    msg.receiver[sizeof(msg.receiver) - 1] = '\0';
    strncpy(msg.symbolVal, symbol, sizeof(msg.symbolVal) - 1);
    msg.symbolVal[sizeof(msg.symbolVal) - 1] = '\0';
    _impl->msgQueue.push(msg);
}

// --- Callbacks ---

void ESPdLib::onFloat(PdFloatCallback callback) {
    if (_impl) _impl->floatCb = callback;
}

void ESPdLib::onBang(PdBangCallback callback) {
    if (_impl) _impl->bangCb = callback;
}

void ESPdLib::onPrint(PdPrintCallback callback) {
    if (_impl) _impl->printCb = callback;
}

// --- Subscribe/Unsubscribe ---

void ESPdLib::subscribe(const char* source) {
    if (!_impl || _impl->numSubs >= PD_MAX_SUBSCRIPTIONS) return;
    void* handle = pdw_bind(source);
    if (handle) {
        _impl->subs[_impl->numSubs].name = source;
        _impl->subs[_impl->numSubs].handle = handle;
        _impl->numSubs++;
    }
}

void ESPdLib::unsubscribe(const char* source) {
    if (!_impl) return;
    for (int i = 0; i < _impl->numSubs; i++) {
        if (strcmp(_impl->subs[i].name, source) == 0) {
            pdw_unbind(_impl->subs[i].handle);
            // Shift remaining entries
            for (int j = i; j < _impl->numSubs - 1; j++) {
                _impl->subs[j] = _impl->subs[j + 1];
            }
            _impl->numSubs--;
            return;
        }
    }
}

// --- Array access ---

int ESPdLib::readArray(float* dest, const char* name, int offset, int n) {
    return pdw_read_array(dest, name, offset, n);
}

int ESPdLib::writeArray(const char* name, int offset, const float* src, int n) {
    return pdw_write_array(name, offset, src, n);
}

// --- Status ---

bool ESPdLib::isRunning() const {
    return _impl && _impl->running;
}

float ESPdLib::getCpuLoad() const {
    return _impl ? _impl->cpuLoad : 0.0f;
}

// --- Filesystem Helpers ---

int ESPdLib::listPatches() {
    if (!_impl) return 0;
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("ESPdLib: LittleFS root not found");
        return 0;
    }
    int count = 0;
    File f = root.openNextFile();
    while (f) {
        String name = f.name();
        if (name.endsWith(".pd")) {
            Serial.printf("  [%d] %s (%d bytes)\n", count + 1, name.c_str(), f.size());
            count++;
        }
        f = root.openNextFile();
    }
    if (count == 0) {
        Serial.println("  (no .pd files found)");
    }
    return count;
}

bool ESPdLib::patchExists(const char* filename) {
    if (!_impl) return false;
    return LittleFS.exists(String("/") + filename);
}

// --- Audio task ---

void ESPdLib::drainMessageQueue() {
    PdMessage msg;
    while (_impl->msgQueue.pop(msg)) {
        switch (msg.type) {
            case PD_MSG_FLOAT:
                pdw_float(msg.receiver, msg.floatVal);
                break;
            case PD_MSG_BANG:
                pdw_bang(msg.receiver);
                break;
            case PD_MSG_SYMBOL:
                pdw_symbol(msg.receiver, msg.symbolVal);
                break;
        }
    }
}

void ESPdLib::audioTaskFunc(void* param) {
    ESPdLib* self = static_cast<ESPdLib*>(param);
    ESPdLibImpl* impl = self->_impl;
    const int outChannels = impl->config.numOutputChannels;
    const int inChannels = impl->config.numInputChannels;
    const int outSamples = PD_BLOCK_SIZE * outChannels;
    const int inSamples = PD_BLOCK_SIZE * inChannels;

    // Use float buffers for Pd processing, then clamp to int16 for I2S.
    // libpd_process_short does NOT clamp — values outside -1..1 overflow int16,
    // causing harsh digital distortion. We process as float and clamp properly.
    float outFloatBuf[PD_BLOCK_SIZE * 2];  // max stereo
    float inFloatBuf[PD_BLOCK_SIZE * 2];   // max stereo
    int16_t outBuffer[PD_BLOCK_SIZE * 2];
    memset(inFloatBuf, 0, sizeof(inFloatBuf));

    // Tick duration in microseconds for CPU load calculation
    const float tickUs = (float)PD_BLOCK_SIZE / impl->config.sampleRate * 1000000.0f;

    while (impl->running) {
        // Drain queued messages from the main thread
        self->drainMessageQueue();

        // Read audio input if configured (convert int16 from I2S to float for Pd)
        if (inChannels > 0) {
            int16_t inI2sBuf[PD_BLOCK_SIZE * 2];
            pd_audio_read(inI2sBuf, inSamples);
            for (int i = 0; i < inSamples; i++) {
                inFloatBuf[i] = inI2sBuf[i] / 32767.0f;
            }
        }

        // Process one Pd tick (64 samples) using float precision
        unsigned long t0 = micros();
        pdw_process_float(1, inFloatBuf, outFloatBuf);
        unsigned long elapsed = micros() - t0;
        impl->cpuLoad = (float)elapsed / tickUs;

        // Convert float (-1.0..1.0) to int16 with clamping
        for (int i = 0; i < outSamples; i++) {
            float s = outFloatBuf[i];
            if (s > 1.0f) s = 1.0f;
            else if (s < -1.0f) s = -1.0f;
            outBuffer[i] = (int16_t)(s * 32767.0f);
        }

        // Write audio output
        if (impl->config.useInternalDAC) {
            pd_audio_write_dac(outBuffer, outSamples);
        } else {
            pd_audio_write(outBuffer, outSamples);
        }
    }

    vTaskDelete(NULL);
}

// --- Serial patch upload ---

void ESPdLib::processSerial() {
    if (!_impl) return;

    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();

        if (!_impl->serialUploading) {
            if (line.startsWith("PATCH_BEGIN ")) {
                _impl->serialFilename = line.substring(12);
                _impl->serialBuffer = "";
                _impl->serialUploading = true;
                Serial.printf("ESPdLib: receiving patch '%s'...\n", _impl->serialFilename.c_str());
            }
        } else {
            if (line == "PATCH_END") {
                File f = LittleFS.open("/" + _impl->serialFilename, "w");
                if (f) {
                    f.print(_impl->serialBuffer);
                    f.close();
                    Serial.printf("ESPdLib: patch '%s' saved (%d bytes)\n",
                                  _impl->serialFilename.c_str(), _impl->serialBuffer.length());
                } else {
                    Serial.printf("ESPdLib: failed to write patch '%s'\n", _impl->serialFilename.c_str());
                }
                _impl->serialUploading = false;
                _impl->serialBuffer = "";
            } else {
                _impl->serialBuffer += line + "\n";
            }
        }
    }
}

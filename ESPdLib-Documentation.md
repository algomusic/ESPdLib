# ESPdLib - Pure Data Engine for ESP32 (Arduino Library)

ESPdLib is an Arduino library that runs the [Pure Data](https://puredata.info/) (Pd) audio engine on an ESP32 microcontroller. It wraps Pd 0.56-2 via the libpd embedding API, outputs audio over I2S, and allows real-time parameter exchange between Arduino code and Pd patches.

## What It Does

- Runs `.pd` patch files on an ESP32 with real-time audio output via I2S
- Patches are stored on LittleFS (ESP32 flash filesystem) and can be hot-swapped at runtime
- Arduino code can send values to Pd `[receive]` objects (e.g. from sensors, pots, GPIO)
- Pd patches can send values back to Arduino via `[send]` objects and callbacks
- Audio processing runs on a dedicated FreeRTOS task at high priority
- Compatible with all ESP32 variants: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│  Arduino Sketch (.ino)                          │
│  - setup(), loop()                              │
│  - Pd.sendFloat("freq", 440)                   │
│  - Pd.openPatch("my-patch.pd")                 │
└────────────────┬────────────────────────────────┘
                 │  Thread-safe message queue (SPSC ring buffer)
                 ▼
┌─────────────────────────────────────────────────┐
│  ESPdLib (ESPdLib.h / ESPdLib.cpp)                    │
│  - C++ wrapper, singleton `Pd`                  │
│  - Manages lifecycle, patches, messaging        │
└────────────────┬────────────────────────────────┘
                 │  C isolation layer (pd_libpd_wrapper.h/.c)
                 ▼
┌─────────────────────────────────────────────────┐
│  libpd / Pure Data Engine                       │
│  - 77 wrapper .c files → 106 Pd source .inc     │
│  - pd_esp32_sched.c (pthread-free scheduler)    │
│  - pd_esp32_stubs.c (stubs for excluded files)  │
└────────────────┬────────────────────────────────┘
                 │  FreeRTOS audio task (core 1)
                 ▼
┌─────────────────────────────────────────────────┐
│  I2S Audio Output (pd_audio.h/.cpp)             │
│  - ESP-IDF i2s_std driver via ArduinoCore       │
│  - 16-bit stereo, configurable sample rate      │
│  - DMA: 8 buffers × 128 samples                 │
└─────────────────────────────────────────────────┘
```

## File Structure

```
ESPdLib/
├── library.properties              # Arduino library metadata
├── ESPdLib-Documentation.md          # This file
├── src/
│   ├── ESPdLib.h                     # Public C++ API (the main interface)
│   ├── ESPdLib.cpp                   # Implementation: init, audio task, messaging
│   ├── pd_audio.h                  # I2S audio driver interface
│   ├── pd_audio.cpp                # I2S driver using ESP-IDF i2s_std.h
│   ├── pd_build_defines.h          # Compile flags for all Pd source files
│   ├── pd_message_queue.h          # Lock-free SPSC ring buffer (loop→audio task)
│   ├── pd_libpd_wrapper.h          # C isolation layer header (avoids word conflict)
│   ├── pd_libpd_wrapper.c          # C isolation layer (calls z_libpd.h directly)
│   ├── pd_esp32_sched.c            # Pthread-free Pd scheduler for ESP32
│   ├── pd_esp32_stubs.c            # Stubs for excluded Pd files (GUI, net, loader)
│   └── pure_data/
│       ├── pd_d_arithmetic.c        # ─┐
│       ├── pd_d_array.c             #  │ 77 thin wrapper files, each 2 lines:
│       ├── pd_d_ctl.c               #  │   #include "../pd_build_defines.h"
│       ├── ...                       #  │   #include "src/d_arithmetic.inc"
│       └── pd_z_ringbuffer.c        # ─┘
│       └── src/
│           ├── m_pd.h               # ─┐
│           ├── s_stuff.h             #  │ 21 Pd header files (pristine, unmodified)
│           ├── ...                   # ─┘
│           ├── d_arithmetic.inc      # ─┐
│           ├── d_array.inc           #  │ 106 Pd source files renamed .c → .inc
│           ├── ...                   #  │ (pristine copies from pd-0.56-2/src/)
│           └── z_ringbuffer.inc      # ─┘
├── examples/
│   ├── SimpleExample/
│   │   ├── SimpleExample.ino        # Main example with pot control + serial switching
│   │   └── data/
│   │       ├── simple-sinewave.pd   # Sine wave patch using [osc~]
│   │       └── simple-phasor.pd     # Phasor patch with [lop~] filter
│   └── GpioControl/
│       ├── GpioControl.ino          # Multi-oscillator example with GPIO + LED
│       └── data/
│           └── gpio-patch.pd
└── scripts/
    ├── update_pd.sh                 # Update Pd sources from a new version
    ├── upload_patch.py              # Send .pd files to ESP32 over serial
    └── embed_patches.py             # (Optional) generate embedded patch header
```

## Key Design Decisions

### Why .inc files and wrapper .c files?

Arduino IDE compiles every `.c` file it finds but provides no way to pass per-library compiler flags. Pd requires specific defines (`-DPD`, `-DPD_INTERNAL`, etc.) to compile.

Solution: Pd source files are renamed `.c` → `.inc` so Arduino ignores them. Each has a thin 2-line wrapper `.c` file that `#include`s the build defines first, then the `.inc` file:

```c
// pd_d_arithmetic.c
#include "../pd_build_defines.h"
#include "src/d_arithmetic.inc"
```

This keeps the original Pd sources completely unmodified for easy version updates.

### Why the C isolation layer (pd_libpd_wrapper)?

Arduino's `Arduino.h` defines `typedef unsigned int word;` and `#define word(...)`. Pd's `m_pd.h` defines `union word`. These conflict in C++ when both headers are visible.

Solution: `pd_libpd_wrapper.c` is a pure C file that includes `z_libpd.h` directly (no Arduino.h conflict in C). It exposes all libpd functions as `pdw_*()` wrappers. `ESPdLib.cpp` only includes `pd_libpd_wrapper.h` (never z_libpd.h), keeping the two type systems separated.

### Why a custom scheduler (pd_esp32_sched.c)?

Pd's `m_sched.c` uses pthreads internally. ESP32's Arduino core runs `setup()`/`loop()` on a FreeRTOS task that isn't registered with the ESP32 pthread layer, causing a `pthread_self` assertion crash.

Solution: `pd_esp32_sched.c` replaces `m_sched.c` with a pthread-free implementation based on the ESPD reference project. It provides all clock functions (`clock_new`, `clock_set`, `clock_delay`, etc.) and `sched_tick()` using direct access to Pd's internal time structures.

### Why stubs (pd_esp32_stubs.c)?

Five Pd source files can't compile on ESP32 due to missing system headers or functionality:
- `s_inter.c` — GUI interaction, `sys/mman.h`
- `s_main.c` — standalone Pd `main()`, not needed with libpd
- `s_loader.c` — dynamic library loading (`dlopen`)
- `s_net.c` — socket networking
- `x_net.c` — Pd `[netsend]`/`[netreceive]` objects

These are excluded (no wrapper files), and `pd_esp32_stubs.c` provides empty/minimal implementations for the symbols they would have defined.

### What's excluded?

- **FFT** — `d_fft.c` and `d_fft_fftsg.c` excluded to save code space (~40KB). `d_fft_setup()` is stubbed empty.
- **Extra objects** — `bob~`, `bonk~`, `fiddle~`, `loop~`, `lrshift~`, `sigmund~`, `stdout` excluded
- **MIDI hardware** — `s_midi.c` excluded (replaced by libpd's `s_libpdmidi.c`)
- **Networking** — all socket/network objects excluded

## Public API Reference

### Initialization

```cpp
ESPdLib::Config config;
config.sampleRate = 48000;        // Audio sample rate
config.numOutputChannels = 2;     // Stereo output
config.numInputChannels = 0;      // No input (set >0 for mic/line-in)
config.bclkPin = 38;              // I2S bit clock pin
config.wsPin = 39;                // I2S word select pin
config.doutPin = 40;              // I2S data out pin
config.dinPin = -1;               // I2S data in pin (-1 = disabled)
config.audioTaskCore = 1;         // FreeRTOS core for audio
config.audioTaskPriority = 20;    // Task priority (high)
config.audioTaskStack = 8192;     // Stack size in bytes
config.usePSRAM = true;           // Route large allocations to PSRAM
config.psramMinAllocSize = 512;   // Threshold in bytes (default 512)

Pd.begin(config);                 // Initialize everything
Pd.end();                         // Shutdown and release resources
```

### Patch Management

```cpp
void* patch = Pd.openPatch("my-patch.pd");        // Load from LittleFS
void* patch = Pd.openPatch("my-patch.pd", "/sd");  // Load from SD card mount
Pd.closePatch(patch);                               // Close/unload
```

### Sending Messages TO Pd

These are thread-safe — call from `loop()` or any task. Messages are queued and delivered to Pd before each audio tick.

```cpp
Pd.sendFloat("freq", 440.0);        // → Pd [r freq] receives 440
Pd.sendBang("trigger");             // → Pd [r trigger] receives bang
Pd.sendSymbol("mode", "square");    // → Pd [r mode] receives "square"
```

### Receiving Messages FROM Pd

Callbacks fire from the audio task thread. Keep them fast.

```cpp
Pd.onFloat([](const char* source, float value) {
    Serial.printf("Pd sent %s = %f\n", source, value);
});
Pd.onBang([](const char* source) {
    Serial.printf("Pd sent bang on %s\n", source);
});
Pd.onPrint([](const char* msg) {
    Serial.printf("[Pd] %s", msg);
});

// Must subscribe to sources before receiving
Pd.subscribe("level");     // Now [s level] in Pd will trigger onFloat
Pd.unsubscribe("level");   // Stop receiving
```

### Array/Table Access

```cpp
float buf[128];
Pd.readArray(buf, "my-table", 0, 128);    // Read 128 samples from Pd array
Pd.writeArray("my-table", 0, buf, 128);   // Write 128 samples to Pd array
```

### Filesystem Helpers

```cpp
Pd.listPatches();                  // Print all .pd files on LittleFS to Serial
Pd.patchExists("my-patch.pd");    // Returns true if file exists on LittleFS
```

### Serial Patch Upload

```cpp
// Call in loop() to listen for PATCH_BEGIN/PATCH_END serial protocol
Pd.processSerial();
```

### Status

```cpp
Pd.isRunning();    // true if audio task is active
Pd.getCpuLoad();   // Fraction of audio deadline used (0.0 - 1.0+)
```

## Pd Patch Design for ESP32

Patches must use `[receive]` (or `[r]`) objects to accept values from Arduino code:

```
[r freq]      ← Pd.sendFloat("freq", 440)
|
[osc~]
|
[*~]
|             [r amp]    ← Pd.sendFloat("amp", 0.5)
|             |
[dac~]        [*~ ]
```

To send values back to Arduino, use `[send]` (or `[s]`) objects and subscribe in the sketch:

```
[adc~]
|
[env~]
|
[s level]     → triggers onFloat callback if subscribed
```

Key constraints:
- No GUI objects (`floatatom`, `vu`, etc.) — there's no GUI on ESP32. Use `[r name]` instead.
- No `[netsend]`/`[netreceive]` — networking objects are excluded
- No FFT objects (`[rfft~]`, `[ifft~]`, etc.) — FFT is excluded to save code space
- No external libraries (`[declare -lib ...]`) — dynamic loading is excluded

## Getting Patches onto the ESP32

### Method 1: LittleFS Upload Tool (Recommended)

1. Install the Arduino LittleFS upload plugin:
   ```bash
   mkdir -p ~/.arduinoIDE/plugins
   # Download .vsix from https://github.com/earlephilhower/arduino-littlefs-upload/releases
   cp ~/Downloads/arduino-littlefs-upload-*.vsix ~/.arduinoIDE/plugins/
   # Restart Arduino IDE
   ```

2. Place `.pd` files in the sketch's `data/` folder

3. **Close the Serial Monitor** (the upload will fail if the port is busy)

4. In Arduino IDE: `Cmd+Shift+P` → "Upload LittleFS to Pico/ESP8266/ESP32"

5. The `data/` folder contents are flashed to the LittleFS partition

### Method 2: Serial Upload (No Recompile)

Requires `pyserial`: `pip3 install pyserial`

```bash
python3 scripts/upload_patch.py /dev/cu.usbmodem* my-patch.pd
python3 scripts/upload_patch.py /dev/cu.usbmodem* data/*.pd   # upload all
```

The sketch must call `Pd.processSerial()` in `loop()` for this to work.

### Method 3: Embedded Fallback

A default `simple-sinewave.pd` patch is embedded as a C string in the SimpleExample sketch. If no `.pd` files exist on LittleFS at boot, this patch is auto-written. This ensures the board always produces audio out of the box.

## Runtime Patch Switching

The SimpleExample sketch supports hot-swapping patches via the Arduino Serial Monitor:

1. Open Serial Monitor (115200 baud)
2. At startup, available patches are listed with numbers:
   ```
   Available patches on LittleFS:
     [1] simple-phasor.pd
     [2] simple-sinewave.pd
   Type a number + Enter in Serial Monitor to switch.
   ```
3. Type a number and press Enter to switch to that patch
4. The current patch is closed and the new one opens immediately

## Updating the Pd Engine Version

When a new version of Pure Data is released (e.g. `pd-0.57-0`):

### Step 1: Run the update script

```bash
cd ESPdLib
./scripts/update_pd.sh /path/to/pd-0.57-0
```

This replaces all `.inc` and `.h` files in `src/pure_data/src/` with the new version's sources.

### Step 2: Check for added/removed source files

Compare the new Pd version's source file list against the existing wrapper files:

```bash
# List current wrappers
ls src/pure_data/pd_*.c | sed 's/.*pd_//' | sed 's/\.c//' | sort > /tmp/wrappers.txt

# List new source files
ls src/pure_data/src/*.inc | sed 's/.*\///' | sed 's/\.inc//' | sort > /tmp/sources.txt

# Find files that need new wrappers
diff /tmp/wrappers.txt /tmp/sources.txt
```

For each **new** source file (e.g. `d_newdsp.inc`), create a wrapper:

```c
// src/pure_data/pd_d_newdsp.c
#include "../pd_build_defines.h"
#include "src/d_newdsp.inc"
```

For each **removed** source file, delete its wrapper.

### Step 3: Compile and fix issues

New Pd versions may:
- Use new system calls → add stubs in `pd_esp32_stubs.c`
- Need new defines → update `pd_build_defines.h`
- Change internal structures → update `pd_esp32_sched.c` if clock/scheduler internals changed
- Add new setup functions → check if `m_conf.c` calls new `*_setup()` functions that need stubs

### Files that may need updating

| File | When to update |
|------|---------------|
| `pd_build_defines.h` | New compile flags needed by new Pd version |
| `pd_esp32_stubs.c` | New symbols referenced from excluded files |
| `pd_esp32_sched.c` | Pd changes its internal clock/scheduler structures |
| `pd_libpd_wrapper.h/.c` | libpd API changes (new functions added) |
| `src/pure_data/pd_*.c` | Source files added or removed |

### Files that should NOT need changes

| File | Why |
|------|-----|
| `ESPdLib.h` / `ESPdLib.cpp` | Uses stable pdw_* wrapper layer |
| `pd_audio.h` / `pd_audio.cpp` | Independent of Pd version |
| `pd_message_queue.h` | Independent of Pd version |

## Build Details

### Compile Flags (pd_build_defines.h)

```c
#define PD                    // Building Pure Data
#define USEAPI_DUMMY          // Dummy audio API (we handle I2S ourselves)
#define PD_INTERNAL           // Access to internal Pd structures
#define HAVE_UNISTD_H         // ESP32 newlib has unistd.h
#define HAVE_ALLOCA_H         // ESP32 newlib has alloca.h
#define PD_HEADLESS           // No GUI
#define SYMTABHASHSIZE 512    // Reduced symbol table (saves RAM)
#define PD_NO_FFT             // Exclude FFT objects
#define lstat stat            // ESP32 newlib has no lstat
#define signal(sig, handler) ((void)(sig), (void)(handler), (void (*)(int))0)
                              // ESP32 newlib declares but doesn't link signal()
```

### Binary Size

Typical build (ESP32-S3, Arduino IDE):
- Flash: ~854 KB (65% of 1.3 MB app partition)
- RAM: ~28 KB (8% of 327 KB)
- Free heap at runtime: ~240 KB+

### Partition Scheme

Use a partition scheme with a LittleFS/SPIFFS partition. In Arduino IDE:
Tools → Partition Scheme → "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"

Despite the name mentioning "spiffs", LittleFS uses the same partition.

### ESP32 Variant Compatibility

ESPdLib runs on all ESP32 variants that support I2S audio output:

| Variant | Cores | Status | Notes |
|---------|-------|--------|-------|
| ESP32 | 2 | Works | Audio task on core 1, loop() on core 0 |
| ESP32-S3 | 2 | Works | Primary development target |
| ESP32-S2 | 1 | Works | Audio task shares core 0 with loop() |
| ESP32-C3 | 1 | Works | RISC-V, single core, less headroom |
| ESP32-C6 | 1 | Works | RISC-V, single core, less headroom |

The audio task core is auto-detected at compile time:
- **Dual-core** (ESP32, S3): audio task runs on core 1, leaving core 0 free for `loop()`, WiFi, etc.
- **Single-core** (S2, C3, C6): audio task runs on core 0 alongside everything else. The high priority (20) ensures audio preempts `loop()`, but complex patches may run out of CPU time. Keep patches simple on single-core chips.

You can override the default with `config.audioTaskCore = 0;` if needed.

**Requirements:**
- Arduino-ESP32 core **v3.x** (based on ESP-IDF 5.x) — needed for the `driver/i2s_std.h` API
- I2S pins must match your specific board/variant

### PSRAM Support

ESP32 boards with PSRAM (common on ESP32-S3-WROOM-1, ESP32-WROVER modules) can use it
to handle larger patches with big `[table]`/`[array]` objects, long delay lines, or samplers.

**Setup:**

1. Enable PSRAM in Arduino IDE: **Tools → PSRAM → "OPI PSRAM"** (S3) or **"Enabled"** (ESP32)

2. Enable in your sketch config:
   ```cpp
   config.usePSRAM = true;            // Enable PSRAM for large allocations
   config.psramMinAllocSize = 512;    // Threshold (default 512 bytes)
   ```

**How it works:**

When enabled, `begin()` calls `heap_caps_malloc_extmem_enable()` which tells ESP-IDF's
heap allocator to place any `malloc()` allocation >= the threshold size into PSRAM. Since
Pd uses `malloc()` internally (via `getbytes()`), this automatically routes large Pd
allocations (arrays, tables, audio buffers) to PSRAM while keeping small, frequently-accessed
structures (object pointers, DSP graph nodes) in fast internal SRAM.

**What benefits from PSRAM:**
- Pd `[table]` and `[array]` objects (wavetables, sample buffers)
- `[delwrite~]` / `[delread~]` delay lines
- Large symbol tables from complex patches
- Audio task stack (if `audioTaskStack` is set large)

**What stays in internal SRAM** (automatically, due to small allocation size):
- Per-object structs (~50-200 bytes each)
- DSP graph connections
- Signal vectors for the current block (64 × 4 bytes = 256 bytes per signal)

**Performance note:** PSRAM is slower than internal SRAM. ESP32-S3 has fast octal-SPI
PSRAM (~120 MB/s) which handles audio well. Standard ESP32 has slower quad-SPI PSRAM
(~25 MB/s) — heavy DSP patches may see increased CPU load. Monitor with `Pd.getCpuLoad()`.

Serial output when PSRAM is enabled:
```
ESPdLib: PSRAM enabled (8192 KB available, threshold >= 512 bytes)
ESPdLib: initialized
ESPdLib: free internal RAM: 240 KB, free PSRAM: 8100 KB
```

## Troubleshooting

### "ESPdLib.h: No such file or directory"

The library isn't installed. Either:
- Copy/symlink `ESPdLib/` to `~/Documents/Arduino/libraries/ESPdLib`
- Or use Arduino IDE Library Manager (if published)

### "pthread_self assertion failed"

The Pd scheduler is using pthreads. This means `pd_m_sched.c` (the wrapper for Pd's `m_sched.c`) was accidentally included. Delete it — `pd_esp32_sched.c` replaces it.

### "open: /littlefs/my-patch.pd: No such file or directory"

The patch file isn't on LittleFS. Upload it using the LittleFS upload tool or serial upload.

### Port busy when uploading LittleFS

Close the Arduino Serial Monitor before using the LittleFS upload tool.

### Audio glitches or silence

- Check I2S pin assignments match your hardware
- Verify `config.sampleRate` matches your DAC's capabilities
- Check `Pd.getCpuLoad()` — if it's above 1.0, the patch is too heavy
- Ensure the patch has `[dac~]` with connections to both outlets (stereo)

### Noisy potentiometer readings

ESP32 ADC is inherently noisy. Use software smoothing:
```cpp
float freq = (prevFreq * 9 + newReading) / 10.0;  // Simple low-pass filter
```

## Known Limitations

- **Single Pd instance** — `PDINSTANCE` is not enabled; one Pd engine runs at a time
- **No FFT** — `[rfft~]`, `[ifft~]`, `[fft~]` objects are excluded
- **No externals** — dynamic library loading (`dlopen`) is not available on ESP32
- **No networking** — `[netsend]`, `[netreceive]` objects are excluded
- **No MIDI hardware** — MIDI I/O objects exist but aren't connected to hardware
- **No GUI** — headless only; use `[r name]`/`[s name]` for all parameter exchange
- **Message queue capacity** — 64 messages buffered between `loop()` and audio task. If `sendFloat()` is called faster than the audio task drains them, messages are dropped silently.

## Reference Implementation

This library is based on the ESPD reference project at `2025.03.02.espd/` which runs Pd on ESP32 using ESP-IDF directly. ESPdLib adapts this for the Arduino IDE ecosystem using the libpd embedding API for a cleaner, more maintainable interface.

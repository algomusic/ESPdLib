# ESPdLib

An Arduino library that runs [Pure Data](https://puredata.info/) (Pd) patches on ESP32 microcontrollers with I2S audio output.

Build audio synthesizers, effects, and interactive sound installations by designing patches in Pd on your computer, then running them on an ESP32 with real-time parameter control from GPIO, sensors, or serial.

## Features

- Load and hot-swap `.pd` patches stored on LittleFS flash
- Send/receive floats, bangs, and symbols between Arduino code and Pd patches
- Stereo I2S audio output (16-bit, configurable sample rate)
- Dedicated FreeRTOS audio task for glitch-free playback
- Thread-safe message queue for control from `loop()`
- PSRAM support for large tables, delay lines, and samplers
- Works on all ESP32 variants (ESP32, S2, S3, C3, C6)

## Quick Start

```cpp
#include <ESPdLib.h>

void setup() {
    ESPdLib::Config config;
    config.sampleRate = 48000;
    config.bclkPin = 38;
    config.wsPin = 39;
    config.doutPin = 40;

    Pd.begin(config);
    Pd.openPatch("my-patch.pd");
    Pd.sendFloat("freq", 440.0);
}

void loop() {
    float freq = analogRead(1) / 4095.0 * 2000.0;
    Pd.sendFloat("freq", freq);
    delay(20);
}
```

## Getting Patches onto the ESP32

1. **LittleFS Upload Tool** (recommended) -- Place `.pd` files in your sketch's `data/` folder, then use `Cmd+Shift+P` > "Upload LittleFS to Pico/ESP8266/ESP32". Requires the [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) plugin.

2. **Serial Upload** -- Send patches at runtime without recompiling: `python3 scripts/upload_patch.py /dev/cu.usbmodem* my-patch.pd`

3. **Embedded Fallback** -- A default sinewave patch is compiled into the EmbeddedExample sketch and auto-written to LittleFS if no patches are found.

## Pd Patch Requirements

Patches must use `[receive]` objects (not GUI elements) to accept values from Arduino code:

```
[r freq] --> [osc~] --> [*~] --> [dac~]
                        [r amp] --^
```

No FFT, networking, external libraries, or GUI objects -- headless audio only.

## Hardware

- Any ESP32 board with I2S output (ESP32, S2, S3, C3, C6)
- I2S DAC module (MAX98357A, PCM5102, UDA1334A, etc.)
- Configurable I2S pins via `config.bclkPin`, `config.wsPin`, `config.doutPin`

## Requirements

- Arduino IDE 2.x
- Arduino-ESP32 core v3.x (ESP-IDF 5.x based)
- LittleFS partition in flash (default partition schemes include one)

## Documentation

See [ESPdLib-Documentation.md](ESPdLib-Documentation.md) for the full API reference, architecture details, PSRAM configuration, Pd engine update instructions, and troubleshooting guide.

## License

Based on Pure Data by Miller Puckette. Pd is released under the BSD license.

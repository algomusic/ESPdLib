# ESPdLib

An Arduino library that runs [Pure Data](https://puredata.info/) (Pd) patches on ESP32 microcontrollers with I2S audio output.

Build audio synthesisers, effects, and interactive sound installations by designing patches in Pd on your computer, then running them on an ESP32 with real-time parameter control from GPIO, sensors, or serial.

Adapted for the Arduino IDE from [ESPd](https://msp.ucsd.edu/ideas/2024.09.18.espd/index.htm) by Miller Puckette.

## Features

- Load and hot-swap `.pd` patches stored on LittleFS flash
- Send/receive floats, bangs, and symbols between Arduino code and Pd patches
- Stereo I2S audio output (16-bit, configurable sample rate)
- Internal DAC output on ESP32/ESP32-S2 (8-bit, no external hardware needed)
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

No FFT, networking, external libraries, or GUI objects -- headless audio only. Sub patches and abstractions are supported.

## Hardware

### External I2S DAC (default)

- Any ESP32 board (ESP32, S2, S3, C3, C6)
- I2S DAC module (MAX98357A, PCM5102, UDA1334A, etc.)
- Configurable I2S pins via `config.bclkPin`, `config.wsPin`, `config.doutPin`
- Optional ES8311 DAC, ES7210 microphones and TCA9555 amplifier control.

### Internal DAC (no external hardware)

- ESP32 (GPIO25 = left, GPIO26 = right) or ESP32-S2 (GPIO17 = left, GPIO18 = right)
- Set `config.useInternalDAC = true` — I2S pin settings are ignored
- 8-bit output resolution (lower quality than external I2S DAC)
- Not available on ESP32-S3, C3, C6, or other chips without a built-in DAC

```cpp
ESPdLib::Config config;
config.useInternalDAC = true;
Pd.begin(config);
```

### I2C Audio Devices

`ESPdLibCodecs.h` provides minimal I2C control for ES8311, ES7210 and TCA9555
without a vendor audio library. Each device is independently optional through
`useES8311Codec`, `useES7210Mic` and `useTCA9555Amp`. These devices are used on
the Waveshare ESP32-S3-AUDIO-Board, but the drivers are not tied to that board;
set the board's I2S and I2C pin numbers in `ESPdLib::Config`. The external-I2S
examples use sketch-level switches so the board pin map stays separate from
the selected audio devices:

```cpp
constexpr bool USE_WAVESHARE_PINOUT = false;
constexpr bool USE_ES8311_CODEC = false;
constexpr bool USE_ES7210_MIC = false;
constexpr bool USE_TCA9555_AMP = false;
```

All switches default to `false`, preserving the standard external I2S DAC
configuration. Set the pinout switch for the Waveshare ESP32-S3-AUDIO-Board
mapping (BCLK=13, WS=14, DOUT=16, DIN=15, MCLK=12); its I2C pins are SDA=11
and SCL=10. Other boards can use the codec or microphone with their own I2S
and I2C pin assignments. Enabling the ES8311 or ES7210 requires 44.1 kHz,
16-bit stereo Philips I2S with 256fs MCLK; the ES7210 enables two input
channels. The `EmbeddedPatch` example demonstrates selecting these settings.

The ES8311 starts at `config.es8311VolumeDb` (-6 dB by default). When enabled,
the TCA9555 amplifier starts disabled and is enabled after device setup
succeeds. The `ESPdLibCodecs` class also exposes runtime controls such as
`es8311SetVolumeDb()`, `es8311Mute()`, `tca9555SpeakerEnable()` and
`es7210SetMicGainDb()`. Keep I2C calls in `setup()` or `loop()`, outside
real-time audio processing.

## Requirements

- Arduino IDE 2.x
- Arduino-ESP32 core v3.x (ESP-IDF 5.x based)
- LittleFS partition in flash (default partition schemes include one)

## Documentation

See [ESPdLib-Documentation.md](ESPdLib-Documentation.md) for the full API reference, architecture details, PSRAM configuration, Pd engine update instructions, and troubleshooting guide.

## License

Based on Pure Data by Miller Puckette. Pd is released under the BSD license.

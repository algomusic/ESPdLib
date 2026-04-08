#include "pd_audio.h"

#ifdef ESP_PLATFORM

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

// Internal DAC support (ESP32 and ESP32-S2 only)
#if defined(SOC_DAC_SUPPORTED) && SOC_DAC_SUPPORTED
#include "driver/dac_continuous.h"
#endif

// --- External I2S DAC state ---

static i2s_chan_handle_t s_tx_handle = NULL;
static i2s_chan_handle_t s_rx_handle = NULL;
static int s_numOutChannels = 2;
static int s_numInChannels = 0;

// --- Internal DAC state ---

#if defined(SOC_DAC_SUPPORTED) && SOC_DAC_SUPPORTED
static dac_continuous_handle_t s_dac_handle = NULL;
// Accumulation buffer: 256 bytes = 128 stereo frames per DMA flush
#define DAC_ACCUM_SIZE 256
#define DAC_DESC_NUM 8
static uint8_t s_dacAccum[DAC_ACCUM_SIZE];
static size_t s_dacAccumPos = 0;
static int s_dacOutChannels = 2;
#endif

// =============================================================================
// External I2S DAC
// =============================================================================

bool pd_audio_init(int sampleRate, int numOutChannels, int numInChannels,
                   int bclkPin, int wsPin, int doutPin, int dinPin) {
    s_numOutChannels = numOutChannels;
    s_numInChannels = numInChannels;

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 128,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };

    esp_err_t err;

    if (numInChannels > 0 && dinPin >= 0) {
        err = i2s_new_channel(&chan_cfg, &s_tx_handle, &s_rx_handle);
    } else {
        err = i2s_new_channel(&chan_cfg, &s_tx_handle, NULL);
    }
    if (err != ESP_OK) return false;

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)sampleRate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                        (numOutChannels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)bclkPin,
            .ws = (gpio_num_t)wsPin,
            .dout = (gpio_num_t)doutPin,
            .din = (dinPin >= 0) ? (gpio_num_t)dinPin : I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(s_tx_handle, &std_cfg);
    if (err != ESP_OK) return false;

    if (s_rx_handle) {
        err = i2s_channel_init_std_mode(s_rx_handle, &std_cfg);
        if (err != ESP_OK) return false;
    }

    err = i2s_channel_enable(s_tx_handle);
    if (err != ESP_OK) return false;

    if (s_rx_handle) {
        err = i2s_channel_enable(s_rx_handle);
        if (err != ESP_OK) return false;
    }

    return true;
}

bool pd_audio_write(const int16_t* samples, size_t numSamples) {
    if (!s_tx_handle) return false;
    size_t bytes = numSamples * sizeof(int16_t);
    size_t written = 0;
    esp_err_t err = i2s_channel_write(s_tx_handle, samples, bytes, &written, portMAX_DELAY);
    return (err == ESP_OK);
}

bool pd_audio_read(int16_t* samples, size_t numSamples) {
    if (!s_rx_handle) return false;
    size_t bytes = numSamples * sizeof(int16_t);
    size_t read_bytes = 0;
    esp_err_t err = i2s_channel_read(s_rx_handle, samples, bytes, &read_bytes, portMAX_DELAY);
    return (err == ESP_OK);
}

void pd_audio_deinit(void) {
    if (s_tx_handle) {
        i2s_channel_disable(s_tx_handle);
        i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
    }
    if (s_rx_handle) {
        i2s_channel_disable(s_rx_handle);
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
    }
}

// =============================================================================
// Internal DAC (ESP32 GPIO25/26, ESP32-S2 GPIO17/18)
// =============================================================================

#if defined(SOC_DAC_SUPPORTED) && SOC_DAC_SUPPORTED

bool pd_audio_init_dac(int sampleRate, int numOutChannels) {
    s_dacOutChannels = numOutChannels;
    s_dacAccumPos = 0;

    dac_continuous_config_t dac_cfg = {
        .chan_mask = DAC_CHANNEL_MASK_ALL,
        .desc_num = DAC_DESC_NUM,
        .buf_size = DAC_ACCUM_SIZE,
        .freq_hz = (uint32_t)sampleRate,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_APLL,
        .chan_mode = DAC_CHANNEL_MODE_ALTER,
    };

    esp_err_t err = dac_continuous_new_channels(&dac_cfg, &s_dac_handle);
    if (err != ESP_OK) return false;

    err = dac_continuous_enable(s_dac_handle);
    if (err != ESP_OK) {
        dac_continuous_del_channels(s_dac_handle);
        s_dac_handle = NULL;
        return false;
    }

    return true;
}

bool pd_audio_write_dac(const int16_t* samples, size_t numSamples) {
    if (!s_dac_handle) return false;

    for (size_t i = 0; i < numSamples; i++) {
        // Convert signed 16-bit to unsigned 8-bit for internal DAC
        s_dacAccum[s_dacAccumPos++] = (uint8_t)((samples[i] + 32768) >> 8);

        // Flush when accumulation buffer is full
        if (s_dacAccumPos >= DAC_ACCUM_SIZE) {
            size_t bytesLoaded = 0;
            size_t remaining = DAC_ACCUM_SIZE;
            uint8_t* ptr = s_dacAccum;
            while (remaining > 0) {
                dac_continuous_write(s_dac_handle, ptr, remaining, &bytesLoaded, portMAX_DELAY);
                ptr += bytesLoaded;
                remaining -= bytesLoaded;
            }
            s_dacAccumPos = 0;
        }
    }

    return true;
}

void pd_audio_deinit_dac(void) {
    if (s_dac_handle) {
        dac_continuous_disable(s_dac_handle);
        dac_continuous_del_channels(s_dac_handle);
        s_dac_handle = NULL;
    }
}

#else
// Stubs for chips without internal DAC (ESP32-S3, C3, etc.)
bool pd_audio_init_dac(int, int) { return false; }
bool pd_audio_write_dac(const int16_t*, size_t) { return false; }
void pd_audio_deinit_dac(void) {}
#endif

#else
// Stubs for non-ESP32 compilation (e.g., desktop testing)
bool pd_audio_init(int, int, int, int, int, int, int) { return false; }
bool pd_audio_write(const int16_t*, size_t) { return false; }
bool pd_audio_read(int16_t*, size_t) { return false; }
void pd_audio_deinit(void) {}
bool pd_audio_init_dac(int, int) { return false; }
bool pd_audio_write_dac(const int16_t*, size_t) { return false; }
void pd_audio_deinit_dac(void) {}
#endif

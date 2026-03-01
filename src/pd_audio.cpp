#include "pd_audio.h"

#ifdef ESP_PLATFORM

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

static i2s_chan_handle_t s_tx_handle = NULL;
static i2s_chan_handle_t s_rx_handle = NULL;
static int s_numOutChannels = 2;
static int s_numInChannels = 0;

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
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
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

#else
// Stubs for non-ESP32 compilation (e.g., desktop testing)
bool pd_audio_init(int, int, int, int, int, int, int) { return false; }
bool pd_audio_write(const int16_t*, size_t) { return false; }
bool pd_audio_read(int16_t*, size_t) { return false; }
void pd_audio_deinit(void) {}
#endif

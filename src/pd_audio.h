#ifndef PD_AUDIO_H
#define PD_AUDIO_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// --- External I2S DAC ---

// Initialize I2S output (and optionally input) for audio streaming.
// Returns true on success.
bool pd_audio_init(int sampleRate, int numOutChannels, int numInChannels,
                   int bclkPin, int wsPin, int doutPin, int dinPin);

// Write interleaved 16-bit PCM samples to I2S. Blocks until DMA accepts the data.
// numSamples = total sample count (frames * channels).
bool pd_audio_write(const int16_t* samples, size_t numSamples);

// Read interleaved 16-bit PCM samples from I2S input (if configured).
// numSamples = total sample count (frames * channels).
bool pd_audio_read(int16_t* samples, size_t numSamples);

// Shutdown I2S.
void pd_audio_deinit(void);

// --- Internal DAC (ESP32 GPIO25/26, ESP32-S2 GPIO17/18) ---

// Initialize the internal DAC via dac_continuous driver.
// Returns true on success, false if unsupported or failed.
bool pd_audio_init_dac(int sampleRate, int numOutChannels);

// Write interleaved 16-bit PCM samples to the internal DAC.
// Converts int16 to uint8 internally. Blocks until DMA accepts the data.
bool pd_audio_write_dac(const int16_t* samples, size_t numSamples);

// Shutdown internal DAC.
void pd_audio_deinit_dac(void);

#ifdef __cplusplus
}
#endif

#endif // PD_AUDIO_H

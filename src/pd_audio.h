#ifndef PD_AUDIO_H
#define PD_AUDIO_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // PD_AUDIO_H

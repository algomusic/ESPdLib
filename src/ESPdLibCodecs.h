// Minimal I2C control for ES8311, ES7210 and TCA9555 audio hardware.
// I2S clocks and data pins are configured by ESPdLib itself.
#ifndef ESPDLIB_CODECS_H
#define ESPDLIB_CODECS_H

#include <Arduino.h>
#include <Wire.h>

class ESPdLibCodecs {
public:
    static bool beginI2C(int sdaPin = 11, int sclPin = 10) {
        if (!Wire.begin(sdaPin, sclPin)) return false;
        Wire.setClock(400000);
        return true;
    }

    static bool es8311Setup(int sampleRate, int mclkRatio = 256) {
        if (sampleRate != 44100 || mclkRatio != 256) return false;
        if (!writeRegister(0x18, 0x00, 0x1F)) return false;
        delay(20);
        const uint8_t sequence[][2] = {
            {0x00, 0x00}, {0x00, 0x80},
            {0x01, 0x3F}, {0x02, 0x00}, {0x03, 0x10}, {0x04, 0x10},
            {0x05, 0x00}, {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xFF},
            {0x00, 0x80}, {0x09, 0x0C}, {0x0A, 0x0C},
            {0x0D, 0x01}, {0x0E, 0x02}, {0x12, 0x00}, {0x13, 0x10},
            {0x1C, 0x6A}, {0x37, 0x08}, {0x31, 0x00}
        };
        if (!writeSequence(0x18, sequence, sizeof(sequence) / sizeof(sequence[0]))) return false;
        return es8311SetVolumeDb(0);
    }

    static bool es7210Setup(int sampleRate, int mclkRatio = 256) {
        if (sampleRate != 44100 || mclkRatio != 256) return false;
        const uint8_t sequence[][2] = {
            {0x00, 0xFF}, {0x00, 0x32},
            {0x09, 0x30}, {0x0A, 0x30},
            {0x23, 0x2A}, {0x22, 0x0A}, {0x21, 0x2A}, {0x20, 0x0A},
            {0x11, 0x60}, {0x12, 0x00},
            {0x40, 0xC3}, {0x41, 0x70}, {0x42, 0x70},
            {0x43, 0x1A}, {0x44, 0x1A}, {0x45, 0x1A}, {0x46, 0x1A},
            {0x47, 0x08}, {0x48, 0x08}, {0x49, 0x08}, {0x4A, 0x08},
            {0x07, 0x20}, {0x02, 0xC1}, {0x04, 0x01}, {0x05, 0x00},
            {0x06, 0x04}, {0x4B, 0x0F}, {0x4C, 0x0F},
            {0x00, 0x71}, {0x00, 0x41},
            {0x1B, 0xBF}, {0x1C, 0xBF}, {0x1D, 0xBF}, {0x1E, 0xBF}
        };
        return writeSequence(0x40, sequence, sizeof(sequence) / sizeof(sequence[0]));
    }

    static bool es8311SetVolumeDb(int db) {
        if (db < -95 || db > 32) return false;
        return writeRegister(0x18, 0x32, (uint8_t)(0xBF + db * 2));
    }

    static bool es8311Mute(bool muted) {
        uint8_t value;
        if (!readRegister(0x18, 0x31, value)) return false;
        value = muted ? (uint8_t)(value | 0x20) : (uint8_t)(value & ~0x20);
        return writeRegister(0x18, 0x31, value);
    }

    // TCA9555 P1.0 (EXIO8) is the speaker amplifier's on/off control.
    static bool tca9555SpeakerEnable(bool enabled) {
        uint8_t output, direction;
        if (!readRegister(0x20, 0x03, output) || !readRegister(0x20, 0x07, direction)) return false;
        output = enabled ? (uint8_t)(output | 0x01) : (uint8_t)(output & ~0x01);
        return writeRegister(0x20, 0x03, output) &&
               writeRegister(0x20, 0x07, (uint8_t)(direction & ~0x01));
    }

    static bool es7210SetMicGainDb(int db) {
        if (db < 0 || db > 36 || (db != 36 && (db > 33 || db % 3 != 0))) return false;
        const uint8_t value = 0x10 | (uint8_t)(db == 36 ? 13 : db / 3);
        for (uint8_t reg = 0x43; reg <= 0x46; ++reg)
            if (!writeRegister(0x40, reg, value)) return false;
        return true;
    }

private:
    static bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
        Wire.beginTransmission(address);
        Wire.write(reg);
        Wire.write(value);
        return Wire.endTransmission() == 0;
    }

    static bool readRegister(uint8_t address, uint8_t reg, uint8_t &value) {
        Wire.beginTransmission(address);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)address, 1) != 1) return false;
        value = (uint8_t)Wire.read();
        return true;
    }

    static bool writeSequence(uint8_t address, const uint8_t (*sequence)[2], size_t count) {
        for (size_t i = 0; i < count; ++i)
            if (!writeRegister(address, sequence[i][0], sequence[i][1])) return false;
        return true;
    }
};

#endif // ESPDLIB_CODECS_H

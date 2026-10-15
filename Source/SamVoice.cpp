#include "SamVoice.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

extern "C"
{
#include "sam.h"
#include "reciter.h"
#include "render.h"
    int debug = 0;
}

int renderSamSpeech (float* dst, int maxLen, double sampleRate,
                     const char* text, int speed, int pitch, int mouth, int throat,
                     int* cut)
{
    if (cut != nullptr)
        *cut = 0;
    if (dst == nullptr || maxLen < 8 || sampleRate < 1000.0)
        return 0;

    char line[256];
    std::memset (line, 0, sizeof line);
    const char* src = (text != nullptr && text[0] != 0) ? text : "ah";
    int n = 0;
    for (int i = 0; src[i] != 0 && n < 48; ++i)
    {
        unsigned char c = (unsigned char) src[i];
        if (c >= 'a' && c <= 'z')
            c = (unsigned char) (c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || c == ' ' || c == '\'' || c == '.' || c == ',' || c == '?' || c == '!')
            line[n++] = (char) c;
    }
    if (n == 0)
    {
        line[0] = 'A';
        line[1] = 'H';
        n = 2;
    }
    line[n] = '[';

    if (! TextToPhonemes ((unsigned char*) line))
    {
        std::memset (line, 0, sizeof line);
        line[0] = 'A';
        line[1] = 'A';
        line[2] = (char) 0x9b;
    }

    SetInput (line);
    const int speedByte = std::clamp (speed, 1, 255);
    const int pitchByte = std::clamp (pitch, 1, 255);
    const int mouthByte = std::clamp (mouth, 0, 255);
    const int throatByte = std::clamp (throat, 0, 255);
    SamSetSpeed ((unsigned char) speedByte);
    SetPitch ((unsigned char) pitchByte);
    SetMouth ((unsigned char) mouthByte);
    SetThroat ((unsigned char) throatByte);
    EnableSingmode();
    samError = 0;
    samOverflow = 0;
    if (! SAMMain() || samError || samOverflow)
    {
        ReleaseSamBuffer();
        if (cut != nullptr)
            *cut = 1;
        return 0;
    }

    const auto* raw = (const unsigned char*) GetBuffer();
    if (raw == nullptr)
        return 0;
    const int samN = std::max (0, GetBufferLength() / 50);
    const int use = std::min (samN + 4, 22050 * 8 - 1);
    const double step = 22050.0 / sampleRate;
    int outN = (use > 1 && step > 0.0) ? (int) std::floor ((double) (use - 1) / step) : 0;
    outN = std::clamp (outN, 0, maxLen);

    float peak = 0.f;
    for (int i = 0; i < outN; ++i)
    {
        const double pos = (double) i * step;
        const int a = (int) pos;
        const int b = std::min (a + 1, use - 1);
        const float frac = (float) (pos - a);
        const float sa = (float) ((int) raw[a] - 128) / 128.f;
        const float sb = (float) ((int) raw[b] - 128) / 128.f;
        dst[i] = sa * (1.f - frac) + sb * frac;
        peak = std::max (peak, std::abs (dst[i]));
    }
    ReleaseSamBuffer();
    if (cut != nullptr)
        *cut = 0;

    const float gain = peak > 0.0001f ? 0.42f / peak : 0.f;
    for (int i = 0; i < outN; ++i)
        dst[i] *= gain;
    return outN >= 2 ? outN : 0;
}

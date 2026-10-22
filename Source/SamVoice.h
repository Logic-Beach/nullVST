#pragma once

#include <cmath>

/* Software Automatic Mouth, 1982, Don't Ask Software (later SoftVoice).
   Sebastian Macke's C port: https://github.com/s-macke/SAM
   speed, pitch, mouth, and throat are that program's own controls.
   pitch is its pitch byte, not a MIDI note. A line that does not fit
   the phoneme tables or the sample buffer is refused.
   noteMatch 1 holds that byte flat, so the fundamental is samFlatHz. */

// One glottal tick steps the buffer by 162/50 samples, read as 22050 Hz.
inline float samFlatHz (int pitchByte)
{
    const int b = pitchByte < 1 ? 1 : (pitchByte > 255 ? 255 : pitchByte);
    return (22050.f * 50.f) / (162.f * (float) b);
}

inline int samByteForHz (float hz)
{
    if (! (hz > 1.f))
        return 64;
    const int b = (int) std::lround ((22050.0 * 50.0) / (162.0 * (double) hz));
    if (b < 1)
        return 1;
    if (b > 255)
        return 255;
    return b;
}

int renderSamSpeech (float* dst, int maxLen, double sampleRate,
                     const char* text, int speed, int pitch, int mouth, int throat,
                     int* cut, int noteMatch = 0);

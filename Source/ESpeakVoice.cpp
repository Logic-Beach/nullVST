#include "ESpeakVoice.h"

#include <espeak-ng/speak_lib.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

namespace
{
std::mutex speakLock;
std::vector<short> chunk;
int engineRate = 0;
bool engineReady = false;

const char* variantName (int variant)
{
    static const char* names[] = {
        "en", "en+m1", "en+m3", "en+m5",
        "en+f1", "en+f3", "en+f5",
        "en+whisper", "en+croak",
        "en+klatt", "en+klatt2", "en+klatt3"
    };
    return names[std::clamp (variant, 0, 11)];
}

int synthCallback (short* wav, int numsamples, espeak_EVENT*)
{
    if (wav != nullptr && numsamples > 0)
        chunk.insert (chunk.end(), wav, wav + numsamples);
    return 0;
}
}

int renderEspeakSpeech (float* dst, int maxLen, double sampleRate,
                        const char* text, int rate, int pitch, int range, int gap, int variant,
                        int* cut, const char* dataPath)
{
    if (cut != nullptr)
        *cut = 0;
    if (dst == nullptr || maxLen < 8 || sampleRate < 1000.0 || dataPath == nullptr || dataPath[0] == 0)
        return 0;

    const std::lock_guard<std::mutex> hold (speakLock);
    if (! engineReady)
    {
        engineRate = espeak_Initialize (AUDIO_OUTPUT_SYNCHRONOUS, 0, dataPath, espeakINITIALIZE_DONT_EXIT);
        if (engineRate <= 0)
            return 0;
        espeak_SetSynthCallback (synthCallback);
        engineReady = true;
    }

    const char* line = (text != nullptr && text[0] != 0) ? text : "ah";
    if (espeak_SetVoiceByName (variantName (variant)) != EE_OK)
        espeak_SetVoiceByName ("en");
    espeak_SetParameter (espeakRATE, std::clamp (rate, 80, 450), 0);
    espeak_SetParameter (espeakPITCH, std::clamp (pitch, 0, 99), 0);
    espeak_SetParameter (espeakRANGE, std::clamp (range, 0, 99), 0);
    espeak_SetParameter (espeakWORDGAP, std::clamp (gap, 0, 20), 0);

    chunk.clear();
    const auto err = espeak_Synth (line, std::strlen (line) + 1, 0, POS_CHARACTER, 0,
                                   espeakCHARS_AUTO, nullptr, nullptr);
    espeak_Synchronize();
    if (err != EE_OK || chunk.size() < 2)
        return 0;

    const double step = (double) engineRate / sampleRate;
    int outN = (int) std::floor ((double) (chunk.size() - 1) / step);
    if (outN > maxLen)
    {
        outN = maxLen;
        if (cut != nullptr)
            *cut = 1;
    }
    outN = std::clamp (outN, 0, maxLen);
    const int last = (int) chunk.size() - 1;

    float peak = 0.f;
    for (int i = 0; i < outN; ++i)
    {
        const double pos = (double) i * step;
        const int a = std::min ((int) pos, last);
        const int b = std::min (a + 1, last);
        const float frac = (float) (pos - a);
        const float sa = (float) chunk[(size_t) a] / 32768.f;
        const float sb = (float) chunk[(size_t) b] / 32768.f;
        dst[i] = sa * (1.f - frac) + sb * frac;
        peak = std::max (peak, std::abs (dst[i]));
    }
    const float gain = peak > 0.0001f ? 0.42f / peak : 0.f;
    for (int i = 0; i < outN; ++i)
        dst[i] *= gain;
    return outN >= 2 ? outN : 0;
}

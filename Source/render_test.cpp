#include "ClicksEngine.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static void writeWav (const std::string& path, const std::vector<float>& mono, int sr)
{
    std::ofstream out (path, std::ios::binary);
    const uint32_t dataBytes = (uint32_t) (mono.size() * 2);
    const uint32_t chunk = 36 + dataBytes;
    const uint16_t audioFormat = 1;
    const uint16_t channels = 1;
    const uint32_t rate = (uint32_t) sr;
    const uint16_t bits = 16;
    const uint32_t byteRate = rate * channels * bits / 8;
    const uint16_t blockAlign = channels * bits / 8;

    out.write ("RIFF", 4);
    out.write (reinterpret_cast<const char*> (&chunk), 4);
    out.write ("WAVE", 4);
    out.write ("fmt ", 4);
    const uint32_t fmtSize = 16;
    out.write (reinterpret_cast<const char*> (&fmtSize), 4);
    out.write (reinterpret_cast<const char*> (&audioFormat), 2);
    out.write (reinterpret_cast<const char*> (&channels), 2);
    out.write (reinterpret_cast<const char*> (&rate), 4);
    out.write (reinterpret_cast<const char*> (&byteRate), 4);
    out.write (reinterpret_cast<const char*> (&blockAlign), 2);
    out.write (reinterpret_cast<const char*> (&bits), 2);
    out.write ("data", 4);
    out.write (reinterpret_cast<const char*> (&dataBytes), 4);
    for (float x : mono)
    {
        const float c = std::clamp (x, -1.f, 1.f);
        const int16_t s = (int16_t) std::lrint (c * 32767.f);
        out.write (reinterpret_cast<const char*> (&s), 2);
    }
}

static std::vector<float> render (ClicksEngine& engine, int note, const ClickParams& params, int samples, int holdSamples = -1)
{
    if (holdSamples < 0)
        holdSamples = samples;
    engine.reset();
    engine.setMix (params);
    engine.noteOn (note, 1.f, params);
    std::vector<float> out ((size_t) samples, 0.f);
    int done = 0;
    bool released = false;
    while (done < samples)
    {
        if (! released && done >= holdSamples)
        {
            engine.noteOff (note);
            released = true;
        }
        const int n = std::min (256, samples - done);
        float* ch = out.data() + done;
        engine.process (&ch, 1, n);
        done += n;
    }
    return out;
}

static double energy (const std::vector<float>& x, int a, int b)
{
    double e = 0;
    b = std::min (b, (int) x.size());
    for (int i = std::max (0, a); i < b; ++i)
        e += (double) x[(size_t) i] * (double) x[(size_t) i];
    return e / std::max (1, b - a);
}

static int bestLag (const std::vector<float>& x, int from, int to, int lag0, int lag1)
{
    int best = lag0;
    double bestScore = -1e300;
    for (int lag = lag0; lag <= lag1; ++lag)
    {
        double score = 0;
        for (int i = from; i < to; ++i)
            score += (double) x[(size_t) i] * (double) x[(size_t) (i + lag)];
        if (score > bestScore)
        {
            bestScore = score;
            best = lag;
        }
    }
    return best;
}

static bool finiteAll (const std::vector<float>& x)
{
    for (float v : x)
        if (! std::isfinite (v))
            return false;
    return true;
}

int main()
{
    const int sr = 44100;
    ClicksEngine engine;
    engine.prepare (sr);

    ClickParams tone;
    tone.voice = 1;
    tone.bits = 8;
    tone.hold = 1;
    tone.glitch = 0;
    tone.slow = 1;
    tone.scatter = 0;
    tone.doublet = 0;
    tone.delaySec = 0;
    tone.echo = 0;
    tone.feedback = 0;
    tone.level = 1;

    const auto a440 = render (engine, 69, tone, sr);
    const auto a220 = render (engine, 57, tone, sr);
    const int lag440 = bestLag (a440, 2500, 7000, 90, 112);
    const int lag220 = bestLag (a220, 2500, 7000, 180, 230);
    const bool pitchOk = std::abs (lag440 - 100) <= 2 && std::abs (lag220 - 200) <= 3;

    const auto again = render (engine, 60, tone, sr / 2);
    const auto same = render (engine, 60, tone, sr / 2);
    float maxDiff = 0;
    for (size_t i = 0; i < again.size(); ++i)
        maxDiff = std::max (maxDiff, std::abs (again[i] - same[i]));

    ClickParams seeded = tone;
    seeded.scatter = 1.f;
    seeded.voice = 0;
    seeded.seed = 7.f;
    seeded.doublet = 0.4f;
    const auto pressA = render (engine, 64, seeded, sr / 4);
    const auto pressB = render (engine, 64, seeded, sr / 4);
    float seedDiff = 0;
    for (size_t i = 0; i < pressA.size(); ++i)
        seedDiff = std::max (seedDiff, std::abs (pressA[i] - pressB[i]));
    seeded.seed = 8.f;
    const auto otherSeed = render (engine, 64, seeded, sr / 4);
    float seedChange = 0;
    for (size_t i = 0; i < pressA.size(); ++i)
        seedChange = std::max (seedChange, std::abs (pressA[i] - otherSeed[i]));
    const bool seedOk = seedDiff == 0.f && seedChange > 0.01f;

    ClickParams echo = tone;
    echo.voice = 1;
    echo.slow = 1;
    echo.glitch = 0;
    echo.delaySec = 1;
    echo.echo = 0.5f;
    echo.feedback = 0;
    echo.attack = 0.002f;
    echo.decay = 0.02f;
    echo.sustain = 0.8f;
    echo.release = 0.04f;
    const auto echoed = render (engine, 60, echo, sr * 2, (int) (0.2 * sr));

    ClickParams held = tone;
    held.attack = 0.002f;
    held.decay = 0.02f;
    held.sustain = 0.9f;
    held.release = 0.03f;
    const auto longNote = render (engine, 60, held, sr, (int) (0.7 * sr));
    const double whileHeld = energy (longNote, (int) (0.45 * sr), (int) (0.65 * sr));
    const double afterRelease = energy (longNote, (int) (0.9 * sr), sr);
    const bool holdOk = whileHeld > afterRelease * 8.0 && whileHeld > 0.01;

    engine.reset();
    ClickParams chord = tone;
    chord.poly = 1;
    engine.setMix (chord);
    engine.noteOn (60, 1.f, chord);
    engine.noteOn (64, 1.f, chord);
    const bool polyOk = engine.activeVoices() == 2;
    chord.poly = 0;
    engine.reset();
    engine.setMix (chord);
    engine.noteOn (60, 1.f, chord);
    engine.noteOn (64, 1.f, chord);
    const bool monoOk = engine.activeVoices() == 1;
    const double direct = energy (echoed, (int) (0.02 * sr), (int) (0.28 * sr));
    const double gap = energy (echoed, (int) (0.55 * sr), (int) (0.9 * sr));
    const double late = energy (echoed, (int) (1.05 * sr), (int) (1.32 * sr));
    const bool echoOk = direct > gap * 8.0 && late > gap * 3.0;

    ClickParams wild;
    wild.voice = 0;
    wild.scatter = 1;
    wild.glitch = 1;
    wild.doublet = 0.4f;
    wild.delaySec = 1;
    wild.echo = 0.5f;
    wild.feedback = 0.3f;
    wild.level = 1;
    const auto shot = render (engine, 64, wild, sr * 3);
    double peak = 0;
    for (float v : shot)
        peak = std::max (peak, (double) std::abs (v));
    const bool alive = finiteAll (shot) && finiteAll (a440) && peak > 0.05 && peak < 2.5;

    writeWav ("clicks-preview.wav", shot, sr);

    std::cout << "lag440 " << lag440 << " lag220 " << lag220
              << " diff " << maxDiff
              << " direct " << direct << " gap " << gap << " late " << late
              << " held " << whileHeld << " released " << afterRelease
              << " peak " << peak
              << " poly " << polyOk << " mono " << monoOk
              << " seedDiff " << seedDiff << " seedChange " << seedChange << "\n";

    if (! pitchOk || maxDiff != 0.f || ! echoOk || ! alive || ! holdOk || ! polyOk || ! monoOk || ! seedOk)
        return 1;
    return 0;
}

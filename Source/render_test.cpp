#include "ClicksEngine.h"
#include "SamVoice.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
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

    auto rises = [] (const std::vector<float>& buf, int a, int b)
    {
        int count = 0;
        a = std::max (1, a);
        b = std::min (b, (int) buf.size());
        for (int i = a; i < b; ++i)
            if (buf[(size_t) i - 1] <= 0.f && buf[(size_t) i] > 0.f)
                ++count;
        return count;
    };
    ClickParams run = tone;
    run.wave = 1;
    run.scatter = 1.f;
    run.scatMode = 1;
    run.steps = 2.f;
    run.slow = 0.5f;
    run.seed = 11.f;
    run.attack = 0.001f;
    run.decay = 0.001f;
    run.sustain = 1.f;
    const auto runA = render (engine, 69, run, sr / 2);
    const auto runB = render (engine, 69, run, sr / 2);
    float runDiff = 0;
    for (size_t i = 0; i < runA.size(); ++i)
        runDiff = std::max (runDiff, std::abs (runA[i] - runB[i]));
    const int slot = sr / 4;
    const int skirt = 400;
    const int lowR = rises (runA, skirt, slot - skirt);
    const int highR = rises (runA, slot + skirt, (int) runA.size() - skirt);
    run.steps = 1.f;
    const auto oneStep = render (engine, 69, run, sr / 2);
    const int oneA = rises (oneStep, skirt, slot - skirt);
    const int oneB = rises (oneStep, slot + skirt, (int) oneStep.size() - skirt);
    const bool runOk = runDiff == 0.f
        && std::abs (lowR - highR) > 8
        && std::abs (oneA - oneB) <= 4;

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

    ClickParams sine = tone;
    sine.wave = 1;
    sine.bits = 8;
    sine.hold = 1;
    sine.glitch = 0;
    sine.scatter = 0;
    sine.doublet = 0;
    const auto sineWave = render (engine, 69, sine, sr);
    const int sineLag = bestLag (sineWave, 2500, 7000, 90, 112);
    const auto sineLock = lockBassScope (sineWave.data() + 3000, 5000, sr, 440.0, 0);
    sine.bits = 3;
    sine.hold = 12;
    const auto crushed = render (engine, 69, sine, sr);
    const auto crushLock = lockBassScope (crushed.data() + 3000, 5000, sr, 440.0, 0);
    int coarse = 0;
    {
        bool seen[21] = {};
        for (int i = 4000; i < 8000; ++i)
        {
            const int bin = std::clamp ((int) std::lround (crushed[(size_t) i] * 10.f) + 10, 0, 20);
            if (! seen[bin])
            {
                seen[bin] = true;
                ++coarse;
            }
        }
    }
    const bool waveOk = std::abs (sineLag - 100) <= 3
        && std::abs (sineLock.period - 100.0) < 8.0
        && sineLock.length > 600 && sineLock.length < 1100
        && std::abs (crushLock.period - 100.0) < 12.0
        && coarse >= 3 && coarse <= 14;

    ClickParams slowed = tone;
    slowed.wave = 1;
    slowed.slow = 0.5f;
    slowed.bits = 8;
    slowed.hold = 1;
    slowed.glitch = 0;
    const auto slowNote = render (engine, 69, slowed, sr);
    const auto slowAgain = render (engine, 69, slowed, sr);
    const int slowLag = bestLag (slowNote, 2500, 7000, 180, 230);
    const int slowLag2 = bestLag (slowAgain, 2500, 7000, 180, 230);
    const bool slowOk = std::abs (slowLag - 200) <= 6 && std::abs (slowLag2 - 200) <= 6;

    std::vector<float> speech ((size_t) (sr * 2), 0.f);
    int cut = 0;
    const int spoken = renderSamSpeech (speech.data(), (int) speech.size(), sr, "hi", 72, 64, 128, 128, &cut);
    engine.setSpeech (speech.data(), spoken, 6805.6f / 64.f, sr);
    ClickParams sam = tone;
    sam.voice = 4;
    sam.wave = 0;
    sam.bits = 8;
    sam.hold = 1;
    sam.glitch = 0;
    sam.scatter = 0;
    sam.doublet = 0;
    sam.slow = 1;
    sam.echo = 0;
    const auto said = render (engine, 57, sam, sr);
    const auto saidLow = render (engine, 45, sam, sr);
    const double saidE = energy (said, (int) (0.05 * sr), (int) (0.45 * sr));
    double saidDiff = 0;
    for (size_t i = 0; i < said.size() && i < saidLow.size(); ++i)
        saidDiff = std::max (saidDiff, (double) std::abs (said[(size_t) i] - saidLow[(size_t) i]));
    double envDot = 0, envA = 0, envB = 0;
    for (int i = 0; i + 256 <= (int) said.size() && i + 256 <= (int) saidLow.size(); i += 256)
    {
        double a = 0, b = 0;
        for (int k = 0; k < 256; ++k)
        {
            a += std::abs (said[(size_t) (i + k)]);
            b += std::abs (saidLow[(size_t) (i + k)]);
        }
        envDot += a * b;
        envA += a * a;
        envB += b * b;
    }
    const double talkMatch = envDot / std::sqrt (std::max (1e-12, envA * envB));

    engine.reset();
    engine.setMix (sam);
    engine.noteOn (57, 1.f, sam);
    std::vector<float> liveNote ((size_t) sr, 0.f);
    std::vector<float> swapped ((size_t) sr, 0.f);
    for (int i = 0; i < (int) swapped.size(); ++i)
        swapped[(size_t) i] = 0.35f * std::sin (2.0 * 3.14159265358979323846 * 880.0 * (double) i / (double) sr);
    const int swapAt = (int) (0.2 * sr);
    int walked = 0;
    bool didSwap = false;
    while (walked < (int) liveNote.size())
    {
        if (! didSwap && walked >= swapAt)
        {
            engine.setSpeech (swapped.data(), (int) swapped.size(), 440.f, (double) sr);
            didSwap = true;
        }
        const int n = std::min (256, (int) liveNote.size() - walked);
        float* ch = liveNote.data() + walked;
        engine.process (&ch, 1, n);
        walked += n;
    }
    double earlySame = 0;
    double lateDiff = 0;
    for (int i = (int) (0.05 * sr); i < (int) (0.15 * sr); ++i)
        earlySame = std::max (earlySame, (double) std::abs (liveNote[(size_t) i] - said[(size_t) i]));
    for (int i = (int) (0.45 * sr); i < (int) (0.7 * sr); ++i)
        lateDiff = std::max (lateDiff, (double) std::abs (liveNote[(size_t) i] - said[(size_t) i]));
    const bool liveOk = earlySame < 0.001 && lateDiff > 0.05 && finiteAll (liveNote);

    ClickParams beat = sam;
    beat.sync = 1;
    beat.restartQuarters = 0.5f;
    beat.slow = 1.f;
    beat.attack = 0.f;
    beat.decay = 0.f;
    beat.sustain = 1.f;
    beat.echo = 0.f;
    engine.setSpeech (speech.data(), spoken, 6805.6f / 64.f, (double) sr);
    engine.setClock (0.0, 120.0, 4, true);
    engine.reset();
    engine.setMix (beat);
    engine.noteOn (57, 1.f, beat);
    const int beatPeriod = 11025;
    std::vector<float> looped ((size_t) (beatPeriod + 1800), 0.f);
    int beatWalk = 0;
    while (beatWalk < (int) looped.size())
    {
        const int n = std::min (256, (int) looped.size() - beatWalk);
        float* ch = looped.data() + beatWalk;
        engine.process (&ch, 1, n);
        beatWalk += n;
    }
    auto windowCorr = [] (const std::vector<float>& x, int a, int b, int n)
    {
        double dot = 0, aa = 0, bb = 0;
        for (int i = 0; i < n; ++i)
        {
            const double u = x[(size_t) (a + i)];
            const double v = x[(size_t) (b + i)];
            dot += u * v;
            aa += u * u;
            bb += v * v;
        }
        return dot / std::sqrt (std::max (1e-12, aa * bb));
    };
    const double restartCorr = windowCorr (looped, 900, beatPeriod + 900, 700);
    const double midCorr = windowCorr (looped, 900, beatPeriod / 2 + 900, 700);
    const bool beatOk = restartCorr > 0.85 && restartCorr > midCorr + 0.15 && finiteAll (looped);

    std::vector<float> flat ((size_t) 2000, 0.45f);
    ClickParams loopedSam = sam;
    loopedSam.attack = 0.f;
    loopedSam.decay = 0.f;
    loopedSam.sustain = 1.f;
    loopedSam.bits = 8;
    loopedSam.hold = 1;
    loopedSam.glitch = 0;
    loopedSam.crushMix = 1.f;
    loopedSam.sync = 0;
    engine.setSpeech (flat.data(), (int) flat.size(), 200.f, (double) sr);
    engine.reset();
    engine.setMix (loopedSam);
    engine.noteOn (57, 1.f, loopedSam);
    std::vector<float> lap ((size_t) 4200, 0.f);
    int lapAt = 0;
    while (lapAt < (int) lap.size())
    {
        const int n = std::min (256, (int) lap.size() - lapAt);
        float* ch = lap.data() + lapAt;
        engine.process (&ch, 1, n);
        lapAt += n;
    }
    double pop = 0;
    for (int i = 1600; i < 2500; ++i)
        pop = std::max (pop, (double) std::abs (lap[(size_t) i] - lap[(size_t) (i - 1)]));
    double midLap = 0;
    for (int i = 900; i < 1400; ++i)
        midLap += std::abs (lap[(size_t) i]);
    midLap /= 500.0;
    const bool loopOk = pop < 0.05 && midLap > 0.05 && finiteAll (lap);

    ClickParams crushSet = tone;
    crushSet.wave = 1;
    crushSet.bits = 2;
    crushSet.hold = 24;
    crushSet.glitch = 0.5f;
    crushSet.crushMix = 0.f;
    const auto crushDry = render (engine, 69, crushSet, sr / 4);
    crushSet.crushMix = 1.f;
    const auto crushWet = render (engine, 69, crushSet, sr / 4);
    double crushDiff = 0;
    for (size_t i = 0; i < crushDry.size() && i < crushWet.size(); ++i)
        crushDiff = std::max (crushDiff, (double) std::abs (crushDry[i] - crushWet[i]));
    const bool wetOk = crushDiff > 0.05 && finiteAll (crushDry) && finiteAll (crushWet);

    std::vector<float> phrase ((size_t) sr, 0.f);
    for (int i = (int) (0.55 * sr); i < (int) (0.90 * sr); ++i)
        phrase[(size_t) i] = 0.5f;
    engine.setSpeech (phrase.data(), sr, 220.f, (double) sr);
    auto playFit = [&] (bool synced)
    {
        ClickParams fit = tone;
        fit.voice = 4;
        fit.sync = synced ? 1 : 0;
        fit.restartQuarters = synced ? 1.f : 0.f;
        fit.attack = 0.f;
        fit.decay = 0.f;
        fit.sustain = 1.f;
        fit.slow = 1.f;
        fit.bits = 8;
        fit.hold = 1;
        fit.glitch = 0.f;
        fit.crushMix = 1.f;
        fit.echo = 0.f;
        engine.setClock (0.0, 120.0, 4, true);
        engine.reset();
        engine.setMix (fit);
        engine.noteOn (57, 1.f, fit);
        std::vector<float> out ((size_t) (0.45 * sr), 0.f);
        int at = 0;
        while (at < (int) out.size())
        {
            const int n = std::min (256, (int) out.size() - at);
            float* ch = out.data() + at;
            engine.process (&ch, 1, n);
            at += n;
        }
        return energy (out, (int) (0.28 * sr), (int) (0.38 * sr));
    };
    const double fitFast = playFit (true);
    const double fitNatural = playFit (false);
    const bool fitOk = fitFast > 0.01 && fitFast > fitNatural * 4.0;

    const bool samOk = saidE > 0.001 && saidDiff > 0.02 && talkMatch > 0.85
        && finiteAll (said) && finiteAll (saidLow) && liveOk && beatOk && loopOk;

    std::vector<float> junk ((size_t) (sr * 2), 0.f);
    int longCut = 0;
    const int denied = renderSamSpeech (junk.data(), (int) junk.size(), sr,
                                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                        255, 64, 128, 128, &longCut);
    const bool denyOk = denied == 0 && longCut == 1;

    ClickParams plain = tone;
    plain.wave = 1;
    plain.bits = 8;
    plain.hold = 1;
    plain.glitch = 0;
    plain.subMode = 1;
    plain.subMix = 0;
    const auto drySub = render (engine, 69, plain, sr);
    plain.subMix = 1.f;
    plain.subTune = -12.f;
    const auto added = render (engine, 69, plain, sr);
    float subDiff = 0.f;
    for (size_t i = 0; i < drySub.size() && i < added.size(); ++i)
        subDiff = std::max (subDiff, std::abs (drySub[i] - added[i]));
    ClickParams click = tone;
    click.subMode = 6;
    click.subMix = 0.8f;
    click.subTune = -12.f;
    const auto bent = render (engine, 60, click, sr / 2);
    click.subMix = 0.f;
    const auto unbent = render (engine, 60, click, sr / 2);
    float fmDiff = 0.f;
    for (size_t i = 0; i < bent.size() && i < unbent.size(); ++i)
        fmDiff = std::max (fmDiff, std::abs (bent[i] - unbent[i]));
    const bool subOk = subDiff > 0.05f && fmDiff > 0.02f && finiteAll (added) && finiteAll (bent);

    ClickParams openTone = tone;
    openTone.wave = 1;
    openTone.bits = 8;
    openTone.hold = 1;
    openTone.glitch = 0;
    const auto openSine = render (engine, 69, openTone, sr);
    const double openE = energy (openSine, 5000, 12000);

    ClickParams low = openTone;
    low.filterType = 1;
    low.filterHz = 180.f;
    low.filterQ = 0.7f;
    low.filterPlace = 0;
    const auto lowSine = render (engine, 69, low, sr);
    const double lowE = energy (lowSine, 5000, 12000);

    ClickParams high = openTone;
    high.filterType = 2;
    high.filterHz = 2500.f;
    high.filterQ = 0.7f;
    const auto highSine = render (engine, 69, high, sr);
    const double highE = energy (highSine, 5000, 12000);

    ClickParams band = openTone;
    band.filterType = 3;
    band.filterHz = 440.f;
    band.filterQ = 2.f;
    const auto bandSine = render (engine, 69, band, sr);
    const double bandE = energy (bandSine, 5000, 12000);
    band.filterHz = 3000.f;
    const auto bandFar = render (engine, 69, band, sr);
    const double bandFarE = energy (bandFar, 5000, 12000);

    ClickParams notch = openTone;
    notch.filterType = 4;
    notch.filterHz = 440.f;
    notch.filterQ = 6.f;
    const auto notchSine = render (engine, 69, notch, sr);
    const double notchE = energy (notchSine, 5000, 12000);

    ClickParams bypass = openTone;
    bypass.filterType = 0;
    bypass.filterHz = 180.f;
    const auto bypassed = render (engine, 69, bypass, sr);
    float offDiff = 0.f;
    for (size_t i = 0; i < openSine.size() && i < bypassed.size(); ++i)
        offDiff = std::max (offDiff, std::abs (openSine[i] - bypassed[i]));

    ClickParams bright = openTone;
    bright.wave = 3;
    bright.bits = 8;
    bright.hold = 1;
    bright.glitch = 0;
    bright.delaySec = 0;
    bright.echo = 0;
    const auto rawSq = render (engine, 69, bright, sr);
    auto edge = [] (const std::vector<float>& x, int a, int b)
    {
        double e = 0;
        b = std::min (b, (int) x.size());
        for (int i = std::max (1, a); i < b; ++i)
        {
            const double d = (double) x[(size_t) i] - (double) x[(size_t) (i - 1)];
            e += d * d;
        }
        return e / std::max (1, b - a);
    };
    const double rawEdge = edge (rawSq, 2000, 6000);

    ClickParams genF = bright;
    genF.filterType = 1;
    genF.filterHz = 300.f;
    genF.filterQ = 0.7f;
    genF.filterPlace = 0;
    genF.delaySec = 0.25f;
    genF.echo = 0.55f;
    genF.feedback = 0.45f;
    const auto atGen = render (engine, 69, genF, sr);
    const double genEdge = edge (atGen, 2000, 6000);
    ClickParams outF = genF;
    outF.filterPlace = 1;
    outF.delaySec = 0.f;
    outF.echo = 0.f;
    outF.feedback = 0.f;
    const auto atOut = render (engine, 69, outF, sr);
    const double outEdge = edge (atOut, 2000, 6000);

    ClickParams ringA = bright;
    ringA.filterType = 6;
    ringA.filterHz = 180.f;
    ringA.filterQ = 6.f;
    ringA.filterPlace = 0;
    ringA.delaySec = 0.2f;
    ringA.echo = 0.7f;
    ringA.feedback = 0.55f;
    const auto ringPre = render (engine, 69, ringA, sr);
    ringA.filterPlace = 1;
    const auto ringPost = render (engine, 69, ringA, sr);
    float placeDiff = 0.f;
    for (int i = sr / 5; i < (int) ringPre.size() && i < (int) ringPost.size(); ++i)
        placeDiff = std::max (placeDiff, std::abs (ringPre[(size_t) i] - ringPost[(size_t) i]));

    ClickParams saw = openTone;
    saw.wave = 2;
    const auto rawSaw = render (engine, 69, saw, sr);
    saw.filterType = 5;
    saw.filterHz = 800.f;
    saw.filterQ = 3.f;
    const auto formed = render (engine, 69, saw, sr);
    float formDiff = 0.f;
    for (size_t i = 0; i < rawSaw.size() && i < formed.size(); ++i)
        formDiff = std::max (formDiff, std::abs (rawSaw[i] - formed[i]));

    ClickParams rung = openTone;
    rung.wave = 5;
    rung.filterType = 6;
    rung.filterHz = 220.f;
    rung.filterQ = 7.f;
    const auto rungOut = render (engine, 69, rung, sr);
    const int resoLag = bestLag (rungOut, 6000, 18000, 170, 240);
    const bool formOk = formDiff > 0.02f && finiteAll (formed)
        && std::abs (resoLag - 200) <= 18 && finiteAll (rungOut);

    ClickParams moved = saw;
    moved.formShift = 0.55f;
    moved.formSpread = 2.f;
    moved.formBreath = 0.8f;
    const auto movedForm = render (engine, 69, moved, sr);
    float shiftDiff = 0.f;
    for (size_t i = 0; i < formed.size() && i < movedForm.size(); ++i)
        shiftDiff = std::max (shiftDiff, std::abs (formed[i] - movedForm[i]));
    ClickParams dull = rung;
    dull.resoDamp = 0.95f;
    dull.resoTune = 7.f;
    dull.resoStack = 0.8f;
    const auto dullOut = render (engine, 69, dull, sr);
    float dampDiff = 0.f;
    for (size_t i = 0; i < rungOut.size() && i < dullOut.size(); ++i)
        dampDiff = std::max (dampDiff, std::abs (rungOut[i] - dullOut[i]));
    const bool colourOk = shiftDiff > 0.02f && dampDiff > 0.02f
        && finiteAll (movedForm) && finiteAll (dullOut);

    ClickParams lfoTone = openTone;
    lfoTone.filterType = 1;
    lfoTone.filterHz = 400.f;
    lfoTone.lfoDest[0] = 1;
    lfoTone.lfoShape[0] = 0;
    lfoTone.lfoHz[0] = 8.f;
    lfoTone.lfoDepth[0] = 0.f;
    const auto lfoStill = render (engine, 69, lfoTone, sr);
    lfoTone.lfoDepth[0] = 1.f;
    const auto lfoMoved = render (engine, 69, lfoTone, sr);
    float lfoDiff = 0.f;
    for (size_t i = 0; i < lfoStill.size() && i < lfoMoved.size(); ++i)
        lfoDiff = std::max (lfoDiff, std::abs (lfoStill[i] - lfoMoved[i]));
    int quarterIndex = 0;
    int barIndex = 0;
    for (int i = 0; i < ClickParams::divisionCount(); ++i)
    {
        if (std::string (ClickParams::divisionName (i)) == "1/4")
            quarterIndex = i;
        if (std::string (ClickParams::divisionName (i)) == "1 bar")
            barIndex = i;
    }
    const double quarterSec = ClickParams::divisionQuarters (quarterIndex, 4) * 60.0 / 120.0;
    const double barSec = ClickParams::divisionQuarters (barIndex, 4) * 60.0 / 120.0;
    const bool lfoOk = lfoDiff > 0.02f && finiteAll (lfoMoved)
        && std::abs (quarterSec - 0.5) < 0.001
        && std::abs (barSec - 2.0) < 0.001;

    const bool filtOk = lowE < openE * 0.12 && highE < openE * 0.02
        && bandE > openE * 0.25 && bandFarE < openE * 0.08
        && notchE < openE * 0.1
        && offDiff == 0.f
        && genEdge < rawEdge * 0.08
        && outEdge < rawEdge * 0.08
        && placeDiff > 0.05f
        && finiteAll (lowSine) && finiteAll (highSine) && finiteAll (bandSine)
        && finiteAll (notchSine) && finiteAll (atGen) && finiteAll (atOut)
        && finiteAll (ringPre) && finiteAll (ringPost)
        && formOk;

    writeWav ("clicks-preview.wav", shot, sr);

    std::cout << "lag440 " << lag440 << " lag220 " << lag220
              << " diff " << maxDiff
              << " direct " << direct << " gap " << gap << " late " << late
              << " held " << whileHeld << " released " << afterRelease
              << " peak " << peak
              << " poly " << polyOk << " mono " << monoOk
              << " seedDiff " << seedDiff << " seedChange " << seedChange
              << " sineLag " << sineLag
              << " scope " << sineLock.period << " x" << sineLock.length
              << " crush " << crushLock.period << " levels " << coarse
              << " samE " << saidE << " samDiff " << saidDiff << " talk " << talkMatch
              << " live " << earlySame << " " << lateDiff
              << " beat " << restartCorr << " " << midCorr
              << " pop " << pop << " wet " << crushDiff
              << " fit " << fitFast << " " << fitNatural
              << " slowLag " << slowLag << " " << slowLag2
              << " cut " << cut
              << " sub " << subDiff << " fm " << fmDiff
              << " low " << lowE << " high " << highE
              << " band " << bandE << " far " << bandFarE
              << " notch " << notchE << " off " << offDiff
              << " genEdge " << genEdge << " outEdge " << outEdge
              << " rawEdge " << rawEdge << " place " << placeDiff
              << " form " << formDiff << " resoLag " << resoLag
              << " shift " << shiftDiff << " damp " << dampDiff
              << " lfo " << lfoDiff << " bar " << barSec
              << " denied " << denied
              << " run " << runDiff << " " << lowR << " " << highR
              << " one " << oneA << " " << oneB << "\n";

    if (! pitchOk || maxDiff != 0.f || ! echoOk || ! alive || ! holdOk || ! polyOk || ! monoOk || ! seedOk || ! waveOk || ! samOk || ! slowOk || ! subOk || ! filtOk || ! denyOk || ! wetOk || ! fitOk || ! lfoOk || ! colourOk || ! runOk)
        return 1;
    return 0;
}

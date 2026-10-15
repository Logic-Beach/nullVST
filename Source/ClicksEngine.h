#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

/* SOUND. Edit this file.
   Knob ranges and names:  Source/PluginProcessor.cpp  (createLayout)
   Panel layout and text:  Source/PluginEditor.cpp

   A key goes down in trigger().
   wave 0 is the old click, kept so older tests still render. The panel
   starts at sine. Sine, saw up, saw down, square, triangle, and noise are generated
   live in process(), so bits and hold have a slope to crush.
   voice 4 is Sam. It plays a sample locked in ahead of time.
   Speed (the slow field) is the playback rate of every note.
   pitchFix 1 keeps speech pitch and only changes the time.
   pitchFix 0 is tape: slowing drops the pitch and roughens the sound.
   samDrag stretches the locked word on the fly. 1 keeps the pace, 0 nearly stops it.
   Warble is the tape stage on the bus: a swaying read head that moves everything
   passing through it, sub, echoes, and speech alike. See tapeStage().
   Repeat moves each source by a fixed run of pitches. The sub stays put there.
   ScatterGlide adds a slow wander to the warble, and slides Repeat between steps.
   SrcGlide slides the source pitch when the key changes. The sub ignores it.
   The sub has its own fine tune, pulse width, and envelope.
   When sync is on, that rate is 1 and the locked Sam sample is fit so
   the phrase fills restartQuarters, then starts again on the beat.
   Delay, attack, decay, and release can each lock to the same divisions.
   Scatter mode 0 warbles: wow at warbleHz, a faster flutter on top, and a
   wander when scatterGlide is up. Depth runs from a hair to six semitones.
   Mode 1 repeats `steps` pitches inside one Speed window:
   the beat division when sync is on, or Speed as seconds when it is free.
   Four LFOs (shape, rate, depth, dest) bend cut, reso, delay, echo, wet,
   glitch, slow, or level. Depth 0 and dest Off leave the knobs alone.
   Retrig on an LFO restarts it at the top of its shape on a new note.
   Retrig on delay clears the echoes. Retrig on speed counts the beat
   division from the note, so speech and Repeat start again with the key.
   sub is a second oscillator on every wave and voice. See shapeSub().
   renderBlip / renderChit write one short loop into a buffer.
   sampleLayer plays that loop.
   degrade is bits, hold, and glitch, on the live sample. Bits run from 2 to 16.
   The scope reads the output, after the limiter, and locks to the
   lowest note. The robot mouth reads Sam's voice only. lockBassScope() finds that window.
   advanceEnv is the attack / decay / sustain / release.
   Levels glide. A new note on a sounding voice, and a wave, filter, or order
   change, crossfade over a few milliseconds. A steady setting stays put.
   process mixes the voices, then the effect chain, then the limiter.
   fxOrder 0 is crush on the voice, then the filter, delay, and compressor.
   filterPlace 0 filters the tone, so the echo repeats that tone.
   filterPlace 1 filters the mix, and the repeats pass through it again.
   Other orders move crush, the filter, delay, tape colour, and the compressor.
   The limiter sits after the output level. limitDb above 0 leaves the ceiling off.
   limitDist 0 only flattens peaks that cross the ceiling. Higher bends the note into it.
   Type 0 is off. 5 is a vowel formant (shift, spread, breath).
   6 is a tuned resonator (damp, tune, octave stack).

   A blip is a pulse. A chit is a shorter pulse; its noise is made
   while it plays, so the noise does not loop. The pulse loop is a
   whole number of cycles, so the join does not thump. Scatter and
   seed pick the shape. The same key and seed repeat. MIDI sets pitch. */

struct ClickParams
{
    int wave = 0;        // 0 click, 1 sine … 5 noise, 6 sam, 7 espeak
    float width = 0.5f;  // pulse width of the main oscillator. 0.5 is the plain shape.
    float samDrag = 1.f; // live stretch of the locked word. 1 keeps the pace, 0 nearly stops it
    int voice = 1;       // kept so older notes still reach Sam. The panel no longer shows it.
    float bits = 4.f;    // quantizer depth, 2..16. See degrade().
    float hold = 14.f;   // samples to freeze each crushed step
    float glitch = 0.1f; // dropouts; also adds extra hold
    int bitsOn = 1;      // 0 skips the quantizer
    int crushOn = 1;     // 0 skips hold and glitch
    float crushMix = 1.f;// 0 dry, 1 full bits / hold / glitch
    float slow = 1.f;    // Speed. Playback rate. Sync forces this to 1.
    int pitchFix = 1;    // 1 keeps speech pitch. 0 is tape: pitch follows speed and the sound roughens
    int sync = 0;        // 1 restarts Sam on restartQuarters
    int slowRetrig = 0;  // 1 counts that division from the note, and restarts Repeat
    float restartQuarters = 0.f; // beat length, in quarter notes. 0 is off.
    float scatter = 1.f; // 0 = sit on the note, 1 = full warble reach (six semitones)
    float seed = 42.f;   // which shape, with the note
    int scatMode = 0;    // 0 warble (tape wow and flutter on the bus), 1 repeat (a fixed run per source)
    float steps = 5.f;   // how many pitches the repeat run holds
    float scatterGlide = 0.f; // warble: seconds per wander step, 0 is pure wow. repeat: slide time between steps
    float warbleHz = 0.f;     // wow rate. 0 falls back to 1.2 Hz. Tape speed does not move it.
    float srcGlide = 0.f;     // seconds to slide source pitch on a new key. 0 is instant
    float doublet = 0.f; // retired. The faster copy is gone.
    float delaySec = 1.f;
    int delayRetrig = 0; // 1 clears the echoes on a new note
    float feedback = 0.3f;
    float echo = 0.0f;   // 0 dry, 1 all echo
    float level = 1.f;   // linear gain, not dB
    float srcLevel = 1.f;    // mixer: main source. 1 is full
    float subLevel = 1.f;    // mixer: sub oscillator. 1 is full
    float crushLevel = 1.f;  // mixer: scales the crush wet. 1 leaves wet as set
    float filtMix = 1.f;     // mixer: 1 is the full filter, 0 leaves it dry
    float delayLevel = 1.f;  // mixer: scales the echo wet. 1 leaves echo as set
    float compMix = 1.f;     // mixer: 1 is the full compressor, 0 leaves it dry
    int poly = 0;        // 0 mono, 1 poly
    float attack = 0.01f;
    float decay = 0.12f;
    float sustain = 0.85f;
    float release = 0.1f;
    int subMode = 0;     // 0 off, 1 add, 2 subtract, 3 multiply, 4 divide, 5 AM, 6 FM, 7 min, 8 max
    int subWave = 0;     // 0 sine, 1 square, 2 saw up, 3 saw down, 4 triangle
    float subTune = -12.f; // semitones from the key
    float subFine = 0.f; // cents on top of subTune. 0 is the semitone
    float subMix = 0.f;  // 0 dry
    float subWidth = 0.5f; // pulse width of the sub. 0.5 is the plain shape
    float subAttack = 0.01f;
    float subDecay = 0.12f;
    float subSustain = 0.85f;
    float subRelease = 0.28f;
    int filterType = 0;  // 0 off, 1 lowpass, 2 highpass, 3 bandpass, 4 notch, 5 formant, 6 resonator
    int filterPlace = 0; // 0 after generation, 1 last, before output. Used when fxOrder is 0.
    int fxOrder = 0;     // 0 is the original chain. Used while the slot numbers stay at 1..6.
    int tapeSlot = 0;    // 0 is first. Each effect has its own place in the chain.
    int crushSlot = 1;
    int filtSlot = 2;
    int delaySlot = 3;
    int revSlot = 4;
    int compSlot = 5;
    float revSize = 0.45f;
    float revDamp = 0.4f;
    float revMix = 0.f;  // 0 leaves the reverb silent
    float filterHz = 4000.f;
    float filterQ = 0.7f;
    float formShift = 1.f;   // scales the vowel. 1 is the written formants
    float formSpread = 1.f;  // 1 keeps the gaps. Higher pulls the peaks apart
    float formBreath = 0.f;  // noise in the vowel. 0 is clean
    float resoDamp = 0.f;    // dulls the ring. 0 is bright
    float resoTune = 0.f;    // semitones from the cutoff
    float resoStack = 0.f;   // octave ring mixed in. 0 is one pitch
    float fAttack = 0.01f;
    float fDecay = 0.18f;
    float fSustain = 0.f;
    float fRelease = 0.2f;
    float filtAmt = 0.f;     // filter envelope depth. 0 leaves the cutoff alone
    float compThresh = 0.f; // dB. Only peaks above this are squeezed
    float compRatio = 1.f;  // 1 leaves the sound alone
    float compGain = 0.f;   // makeup dB after the squeeze. 0 is unity
    float limitDb = 6.f;    // ceiling in dB. Above 0 the limiter stays out. 0 dB holds full scale.
    float limitDist = 0.f;  // 0 is a clean ceiling. 1 bends the note into the clip.
    static constexpr int lfoCount = 4;
    int lfoShape[4] = { 0, 0, 0, 0 }; // 0 sine, 1 triangle, 2 saw up, 3 saw down, 4 square, 5 sample-hold
    float lfoHz[4] = { 2.f, 0.5f, 0.25f, 0.1f };
    int lfoSync[4] = { 0, 0, 0, 0 };
    int lfoRetrig[4] = { 0, 0, 0, 0 }; // 1 restarts that LFO on a new note
    float lfoQuarters[4] = { 1.f, 1.f, 1.f, 1.f };
    float lfoDepth[4] = { 0.f, 0.f, 0.f, 0.f };
    int lfoDest[4] = { 0, 0, 0, 0 }; // 0 off. Same list for every LFO

    // Tempo divisions, short to long. Triplets are marked t, dotted notes with a dot.
    // The free-mode Speed value 1 lands on "1 bar".
    struct Division
    {
        const char* name;
        double beats; // length in quarter notes. 0 when the value follows the bar.
        double bars;  // bar multiplier. 0 for a fixed beat length.
    };

    static constexpr int divisionCount() { return 21; }

    static const Division& division (int index)
    {
        static const Division table[] = {
            { "1/32t",  0.125 * 2.0 / 3.0, 0.0 },
            { "1/32",   0.125, 0.0 },
            { "1/16t",  0.25 * 2.0 / 3.0, 0.0 },
            { "1/32.",  0.125 * 1.5, 0.0 },
            { "1/16",   0.25, 0.0 },
            { "1/8t",   0.5 * 2.0 / 3.0, 0.0 },
            { "1/16.",  0.25 * 1.5, 0.0 },
            { "1/8",    0.5, 0.0 },
            { "1/4t",   2.0 / 3.0, 0.0 },
            { "1/8.",   0.75, 0.0 },
            { "1/4",    1.0, 0.0 },
            { "1/2t",   4.0 / 3.0, 0.0 },
            { "1/4.",   1.5, 0.0 },
            { "1/2",    2.0, 0.0 },
            { "1/2.",   3.0, 0.0 },
            { "1 bar",  0.0, 1.0 },
            { "1 bar.", 0.0, 1.5 },
            { "2 bars", 0.0, 2.0 },
            { "2 bar.", 0.0, 3.0 },
            { "4 bars", 0.0, 4.0 },
            { "8 bars", 0.0, 8.0 },
        };
        static_assert (sizeof (table) / sizeof (table[0]) == 21, "tempo division table");
        return table[std::clamp (index, 0, divisionCount() - 1)];
    }

    static int divisionIndexNorm (float norm)
    {
        const float u = std::clamp (norm, 0.f, 1.f);
        const int last = divisionCount() - 1;
        return std::clamp ((int) std::lround (u * (float) last), 0, last);
    }

    static int divisionIndex (float slow)
    {
        const float u = std::clamp ((slow - 0.25f) / 1.f, 0.f, 1.f);
        return divisionIndexNorm (u);
    }

    static float divisionAnchor (int index)
    {
        const int last = divisionCount() - 1;
        return 0.25f + (float) std::clamp (index, 0, last) / (float) last;
    }

    static const char* divisionName (int index)
    {
        return division (index).name;
    }

    static double divisionQuarters (int index, int beatsPerBar)
    {
        const auto& step = division (index);
        const int beats = std::max (1, beatsPerBar);
        if (step.bars > 0.0)
            return (double) beats * step.bars;
        return step.beats;
    }
};

class ClicksEngine
{
    struct Voice;

public:
    void prepare (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 44100.0;
        cap = (int) std::ceil (sampleRate * 8.0) + 4;
        for (auto& voice : voices)
        {
            voice.active = false;
            voice.lastOut = 0.f;
            voice.fadeLeft = 0;
            voice.nLayers = 0;
            for (auto& layer : voice.layer)
            {
                layer.buf.assign ((size_t) cap, 0.f);
                layer.playing = false;
            }
        }
        delayLen = (int) std::ceil (sampleRate * 16.0) + 4;
        delay.assign ((size_t) delayLen, 0.f);
        write = 0;
        delayReady = false;
        tapeLen = (int) std::ceil (sampleRate * 0.15) + 16;
        tapeRing.assign ((size_t) tapeLen, 0.f);
        clearTape();
        resoLen = (int) std::ceil (sampleRate / 40.0) + 8;
        reso.assign ((size_t) resoLen, 0.f);
        resoWrite = 0;
        filtIc1 = 0.f;
        filtIc2 = 0.f;
        formIc1[0] = formIc1[1] = formIc1[2] = 0.f;
        formIc2[0] = formIc2[1] = formIc2[2] = 0.f;
        resoDull = 0.f;
        filterReady = false;
        for (int i = 0; i < ClickParams::lfoCount; ++i)
        {
            lfoPhase[i] = 0.f;
            lfoHold[i] = 0.f;
        }
        compEnv = 0.f;
        busCrush = {};
        smoothReady = false;
        busFadeLeft = 0;
        busLast = 0.f;
        polyGain = 1.f;
        tuneReverb();
        bendSemis = 0.f;
        bendMul = 1.f;
        monoN = 0;
        scopeWrite = 0;
        scopePub.store (0);
        scopeHz.store (0.f);
    }

    void reset()
    {
        for (auto& voice : voices)
        {
            voice.active = false;
            voice.lastOut = 0.f;
            voice.fadeLeft = 0;
        }
        smoothReady = false;
        busFadeLeft = 0;
        busLast = 0.f;
        polyGain = 1.f;
        std::fill (delay.begin(), delay.end(), 0.f);
        write = 0;
        delayReady = false;
        clearTape();
        std::fill (reso.begin(), reso.end(), 0.f);
        resoWrite = 0;
        filtIc1 = 0.f;
        filtIc2 = 0.f;
        formIc1[0] = formIc1[1] = formIc1[2] = 0.f;
        formIc2[0] = formIc2[1] = formIc2[2] = 0.f;
        resoDull = 0.f;
        filterReady = false;
        for (int i = 0; i < ClickParams::lfoCount; ++i)
        {
            lfoPhase[i] = 0.f;
            lfoHold[i] = 0.f;
        }
        compEnv = 0.f;
        busCrush = {};
        clearReverb();
        monoN = 0;
        scopeWrite = 0;
        scopePub.store (0);
        scopeHz.store (0.f);
        scatterClock = 0;
        scatterOrigin = 0;
        syncOriginPpq = 0;
    }

    void setPitchBend (float semitones)
    {
        bendSemis = std::clamp (semitones, -2.f, 2.f);
        bendMul = std::pow (2.f, bendSemis / 12.f);
    }

    void setMix (const ClickParams& params)
    {
        mix = params;
    }

    // Host clock for the next process() call. host false keeps the free-running beat.
    void setClock (double ppq, double bpm, int, bool host)
    {
        clockBpm = std::clamp (bpm, 20.0, 400.0);
        if (host)
            clockPpq = ppq;
    }

    // The locked Sam sample. A held note reads this buffer, so a new
    // render is heard without letting the key up. Keys do not render speech.
    void setSpeech (const float* src, int length, float refHz, double renderedRate)
    {
        if (src == nullptr || length < 2 || refHz < 20.f)
            return;
        // Three slots: the one playing, the one a held note may still be
        // fading from, and this one. The audio thread never reads this slot.
        const int slot = (speechSlot.load (std::memory_order_acquire) + 1) % 3;
        speech[slot].samples.assign (src, src + length);
        speech[slot].len = length;
        speech[slot].refHz = refHz;
        speech[slot].renderedRate = renderedRate > 1000.0 ? renderedRate : 44100.0;
        const int fadeN = (int) std::lround (speech[slot].renderedRate * 0.012);
        const int nFade = std::min (fadeN, std::max (1, length / 3));
        if (nFade > 1)
        {
            for (int i = 0; i < nFade; ++i)
            {
                const float w = 0.5f * (1.f - std::cos (3.14159265f * (float) i / (float) (nFade - 1)));
                speech[slot].samples[(size_t) i] *= w;
                speech[slot].samples[(size_t) (length - 1 - i)] *= w;
            }
        }
        speechSlot.store (slot, std::memory_order_release);
    }

    void letGo (Voice& voice)
    {
        voice.held = false;
        voice.stage = 3;
        voice.subStage = 3;
    }

    void noteOn (int note, float velocity, const ClickParams& params)
    {
        if (velocity <= 0.f || cap < 16)
            return;

        if (params.poly <= 0)
        {
            for (int i = 1; i < 8; ++i)
            {
                if (voices[i].active && voices[i].held)
                    letGo (voices[i]);
            }
            pushMono (note, velocity);
            trigger (&voices[0], note, velocity, params);
            return;
        }

        monoN = 0;
        Voice* voice = nullptr;
        for (auto& existing : voices)
        {
            if (existing.active && existing.held && existing.note == note)
            {
                voice = &existing;
                break;
            }
        }
        if (voice == nullptr)
            voice = allocate();
        trigger (voice, note, velocity, params);
    }

    void noteOff (int note)
    {
        if (mix.poly <= 0)
        {
            int found = -1;
            for (int i = monoN - 1; i >= 0; --i)
            {
                if (monoNote[i] == note)
                {
                    found = i;
                    break;
                }
            }
            if (found >= 0)
            {
                for (int i = found; i < monoN - 1; ++i)
                {
                    monoNote[i] = monoNote[i + 1];
                    monoVel[i] = monoVel[i + 1];
                }
                --monoN;
            }
            if (monoN <= 0)
            {
                if (voices[0].active && voices[0].held)
                    letGo (voices[0]);
            }
            else if (monoNote[monoN - 1] != voices[0].note)
            {
                trigger (&voices[0], monoNote[monoN - 1], monoVel[monoN - 1], mix);
            }
            return;
        }

        Voice* oldest = nullptr;
        for (auto& voice : voices)
        {
            if (! voice.active || ! voice.held || voice.note != note)
                continue;
            if (oldest == nullptr || voice.born < oldest->born)
                oldest = &voice;
        }
        if (oldest != nullptr)
            letGo (*oldest);
    }

    void releaseAll()
    {
        monoN = 0;
        for (auto& voice : voices)
        {
            if (voice.active && voice.held)
                letGo (voice);
        }
    }

    // Start one voice. Picks blip, chit, or both, then fills its loop.
    void trigger (Voice* voice, int note, float velocity, const ClickParams& params)
    {
        const bool wasLive = voice->active && std::abs (voice->lastOut) > 1.0e-4f;
        const float from = voice->lastOut;
        voice->active = true;
        voice->held = true;
        voice->note = note;
        voice->stage = 0;
        voice->env = 0.f;
        voice->subStage = 0;
        voice->subEnv = 0.f;
        voice->gain = std::pow (std::clamp (velocity, 0.f, 1.f), 0.8f) * 0.55f;
        voice->born = ++bornCounter;
        voice->nLayers = 0;
        voice->holdLeft = 0;
        voice->crushed = 0.f;
        voice->muteLeft = 0;
        voice->stutter = 0.f;
        voice->phase = 0;
        voice->phase2 = 0;
        voice->subPhase = 0;

        const uint32_t userSeed = (uint32_t) std::lround (std::clamp (params.seed, 0.f, 999.f));
        const uint32_t seed = ((uint32_t) note * 1315423911u)
            ^ (userSeed * 747796405u + 0x9E3779B9u);
        voice->hash = seed == 0 ? 1u : seed;
        voice->oscNoise = voice->hash;
        voice->scatRng = voice->hash;
        voice->scatStep = -100000;
        voice->scatMul = 1.f;
        voice->scatTarget = 1.f;
        voice->scatSamples = 0;
        const float keyHz = midiHz (note) * bendMul;
        if (params.srcGlide < 0.001f || voice->playHz < 20.f)
            voice->playHz = keyHz;
        if (wasLive)
        {
            voice->fadeFrom = from;
            voice->fadeSpan = std::max (2, (int) std::lround (sampleRate * 0.004));
            voice->fadeLeft = voice->fadeSpan;
        }
        else
            voice->fadeLeft = 0;
        retrigFromNote (params);
        voice->beatCell = beatCellNow();
        filtStage = 0;
        filtEnv = 0.f;
        const bool sam = params.wave == 6 || params.wave == 7 || params.voice == 4;
        if (sam)
        {
            addSpeech (*voice, midiHz (note));
            return;
        }
        if (params.wave > 0)
            return;

        Rng rng (voice->hash);
        addLayer (*voice, rng, 0, params, midiHz (note), false);
    }

    // One block of audio. Voices, then the filter, then the delay line.
    void process (float* const* outputs, int numChannels, int numSamples)
    {
        if (outputs == nullptr || numChannels <= 0 || numSamples <= 0)
            return;

        const int filtType = std::clamp (mix.filterType, 0, 6);
        const int filtPlace = mix.filterPlace == 1 ? 1 : 0;
        const int fxOrder = std::clamp (mix.fxOrder, 0, 5);
        const int routeSlot[6] = {
            std::clamp (mix.tapeSlot, 0, 5),
            std::clamp (mix.crushSlot, 0, 5),
            std::clamp (mix.filtSlot, 0, 5),
            std::clamp (mix.delaySlot, 0, 5),
            std::clamp (mix.revSlot, 0, 5),
            std::clamp (mix.compSlot, 0, 5)
        };
        const bool stockRoute = routeSlot[0] == 0 && routeSlot[1] == 1 && routeSlot[2] == 2
            && routeSlot[3] == 3 && routeSlot[4] == 4 && routeSlot[5] == 5;
        const bool crushOnVoice = stockRoute && (fxOrder == 0 || fxOrder >= 3);
        tuneTape();
        // Effect order for the custom route. Fixed for the whole block.
        int order[6] = { 0, 1, 2, 3, 4, 5 };
        std::stable_sort (order, order + 6, [&] (int a, int b)
        {
            return routeSlot[a] < routeSlot[b];
        });
        latchLfoBases();
        slewCoef = 1.f - std::exp (-1.f / std::max (1.f, 0.004f * (float) sampleRate));
        polyCoef = 1.f - std::exp (-1.f / std::max (1.f, 0.025f * (float) sampleRate));
        const int clickStamp = mix.wave * 1000003
            + mix.voice * 97
            + filtType * 13
            + filtPlace * 3
            + fxOrder * 17
            + mix.subMode * 19
            + mix.subWave * 23
            + mix.pitchFix * 29
            + (int) std::lround (mix.bits) * 31
            + (int) std::lround (mix.hold) * 37
            + routeSlot[0] * 41 + routeSlot[1] * 43 + routeSlot[2] * 47
            + routeSlot[3] * 53 + routeSlot[4] * 59 + routeSlot[5] * 61;
        if (! smoothReady)
        {
            slewLevel = mix.level;
            slewSrc = std::clamp (mix.srcLevel, 0.f, 1.f);
            slewSubLvl = std::clamp (mix.subLevel, 0.f, 1.f);
            slewEcho = std::clamp (mix.echo, 0.f, 1.f);
            slewDelayLvl = std::clamp (mix.delayLevel, 0.f, 1.f);
            slewFb = std::clamp (mix.feedback, 0.f, 0.99f);
            slewCrush = std::clamp (mix.crushMix, 0.f, 1.f) * std::clamp (mix.crushLevel, 0.f, 1.f);
            slewFilt = std::clamp (mix.filtMix, 0.f, 1.f);
            slewComp = std::clamp (mix.compMix, 0.f, 1.f);
            slewSub = std::clamp (mix.subMix, 0.f, 1.f);
            slewWidth = mix.width;
            slewSubWidth = mix.subWidth;
            slewGlitch = mix.glitch;
            slewLimit = mix.limitDb;
            limitLive = mix.limitDb;
            slewDist = mix.limitDist;
            limitDistLive = mix.limitDist;
            slewRev = std::clamp (mix.revMix, 0.f, 1.f);
            slewRevSize = std::clamp (mix.revSize, 0.f, 1.f);
            slewRevDamp = std::clamp (mix.revDamp, 0.f, 1.f);
            revMixLive = slewRev;
            revSizeLive = slewRevSize;
            revDampLive = slewRevDamp;
            clickStampNow = clickStamp;
            smoothReady = true;
        }
        else if (clickStamp != clickStampNow)
        {
            clickStampNow = clickStamp;
            busFadeFrom = busLast;
            busFadeSpan = std::max (2, (int) std::lround (sampleRate * 0.004));
            busFadeLeft = busFadeSpan;
        }

        if (! delayReady)
        {
            smoothedDelay = std::clamp (lfoBaseDelay, 0.f, 16.f) * (float) sampleRate;
            delayReady = true;
        }
        if (! filterReady)
        {
            smoothedCut = std::clamp (lfoBaseCut, 20.f, (float) (sampleRate * 0.45));
            smoothedQ = std::clamp (lfoBaseQ, 0.3f, 8.f);
            filterReady = true;
        }

        followTone();
        noteBass();

        if (filtType != 5)
        {
            formIc1[0] = formIc1[1] = formIc1[2] = 0.f;
            formIc2[0] = formIc2[1] = formIc2[2] = 0.f;
        }
        if (filtType != 6)
        {
            std::fill (reso.begin(), reso.end(), 0.f);
            resoWrite = 0;
            resoDull = 0.f;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            ++scatterClock;
            applyLfo();
            mix.glitch = chase (slewGlitch, mix.glitch);
            mix.width = chase (slewWidth, mix.width);
            const float subWidthKeep = mix.subWidth;
            mix.subWidth = chase (slewSubWidth, subWidthKeep);
            const float echoNow = chase (slewEcho, std::clamp (mix.echo, 0.f, 1.f));
            const float dryGain = std::cos (echoNow * 1.5707963f);
            const float wetGain = std::sin (echoNow * 1.5707963f) * chase (slewDelayLvl, std::clamp (mix.delayLevel, 0.f, 1.f));
            const float level = chase (slewLevel, mix.level);
            const float feedNow = chase (slewFb, std::clamp (mix.feedback, 0.f, 0.99f));
            const float srcNow = chase (slewSrc, std::clamp (mix.srcLevel, 0.f, 1.f));
            const float subMixNow = chase (slewSub, std::clamp (mix.subMix, 0.f, 1.f));
            const float subLvlNow = chase (slewSubLvl, std::clamp (mix.subLevel, 0.f, 1.f));
            const float crushNow = chase (slewCrush, std::clamp (mix.crushMix, 0.f, 1.f) * std::clamp (mix.crushLevel, 0.f, 1.f));
            const float filtMixNow = chase (slewFilt, std::clamp (mix.filtMix, 0.f, 1.f));
            const float compMixNow = chase (slewComp, std::clamp (mix.compMix, 0.f, 1.f));
            limitLive = chase (slewLimit, mix.limitDb);
            limitDistLive = chase (slewDist, std::clamp (mix.limitDist, 0.f, 1.f));
            revMixLive = chase (slewRev, std::clamp (mix.revMix, 0.f, 1.f));
            revSizeLive = chase (slewRevSize, std::clamp (mix.revSize, 0.f, 1.f));
            revDampLive = chase (slewRevDamp, std::clamp (mix.revDamp, 0.f, 1.f));
            const float targetDelay = std::clamp (mix.delaySec, 0.f, 16.f) * (float) sampleRate;
            advanceFiltEnv();
            const float amt = std::clamp (mix.filtAmt, -1.f, 1.f);
            float cutHz = mix.filterHz;
            if (std::abs (amt) > 1.0e-5f)
                cutHz *= std::pow (2.f, amt * filtEnv * 4.f);
            const float targetCut = std::clamp (cutHz, 20.f, (float) (sampleRate * 0.45));
            const float targetQ = std::clamp (mix.filterQ, 0.3f, 8.f);

            if (mix.sync && mix.restartQuarters > 0.02f)
            {
                const int cell = beatCellNow();
                for (auto& voice : voices)
                {
                    if (! voice.active || voice.beatCell == cell)
                        continue;
                    voice.beatCell = cell;
                    for (int n = 0; n < voice.nLayers; ++n)
                    {
                        Layer& layer = voice.layer[n];
                        if (! layer.speech)
                            continue;
                        const int fadeN = std::max (1, (int) std::lround (sampleRate * 0.012));
                        layer.loopFrom = layer.read;
                        layer.loopHop = layer.hop;
                        layer.loopSpan = fadeN;
                        layer.loopLeft = fadeN;
                        layer.read = 0;
                        layer.hop = 0;
                        layer.skipRead = 0;
                        layer.skipHop = 0;
                        layer.fadeLeft = 0;
                    }
                }
            }
            clockPpq += (clockBpm / 60.0) / sampleRate;

            float x = 0.f;
            float samHeard = 0.f;
            for (auto& voice : voices)
            {
                if (! voice.active)
                    continue;
                const float slowNow = std::clamp (mix.slow, 0.05f, 4.f);
                const int subMode = mix.subMode;
                const float depth = subMixNow;
                const bool subOn = subMode > 0 && depth > 0.0001f;
                float rateMul = 1.f;
                float sub = 0.f;
                if (subOn)
                {
                    sub = subOsc (voice, slowNow) * advanceSubEnv (voice);
                    sub *= subLvlNow;
                    if (subMode == 6)
                        rateMul = std::clamp (1.f + sub * depth * 3.f, 0.08f, 6.f);
                }
                updateScatter (voice);
                const float warble = voice.scatMul;
                float s = 0.f;
                const bool samVoice = mix.wave == 6 || mix.wave == 7 || mix.voice == 4;
                if (! samVoice && mix.wave > 0 && (mix.wave < 6 || mix.wave == 8))
                    s = oscillator (voice, slowNow * rateMul, warble);
                else
                    for (int n = 0; n < voice.nLayers; ++n)
                    {
                        if (! voice.layer[n].playing)
                            continue;
                        if (voice.layer[n].speech)
                            s += sampleSpeech (voice, voice.layer[n], slowNow, rateMul, warble);
                        else
                        {
                            float play = voice.layer[n].rate * slowNow * rateMul * warble;
                            s += sampleLayer (voice.layer[n], play);
                        }
                    }
                s *= srcNow;
                const float env = advanceEnv (voice);
                if (! voice.active && subOn && voice.subEnv > 0.0008f)
                    voice.active = true;
                if (! voice.active)
                    continue;
                // Sub off: crush, then the source envelope. That order is the
                // dry path. Sub on: the source envelope shapes the source, the
                // sub envelope already shaped the sub, then crush hits the mix.
                // Orders that crush the mix skip degrade here and do it later.
                if (! crushOnVoice)
                {
                    s *= env;
                    if (subOn && subMode != 6)
                        s = shapeSub (s, sub, subMode, depth);
                }
                else if (! subOn)
                {
                    const float dry = s;
                    s = degrade (voice, s);
                    s = (dry + (s - dry) * crushNow) * env;
                }
                else
                {
                    s *= env;
                    if (subMode != 6)
                        s = shapeSub (s, sub, subMode, depth);
                    const float dry = s;
                    s = degrade (voice, s);
                    s = dry + (s - dry) * crushNow;
                }
                const float heard = settleVoice (voice, s * voice.gain);
                if (samVoice)
                    samHeard += heard;
                x += heard;
            }
            mix.subWidth = subWidthKeep;

            smoothedCut += 0.01f * (targetCut - smoothedCut);
            smoothedQ += 0.01f * (targetQ - smoothedQ);

            const float crushAmt = crushNow;
            const float slowBus = std::clamp (mix.slow, 0.05f, 4.f);
            auto crushBus = [&] (float s)
            {
                const float crushed = degradeAny (busCrush, s);
                return s + (crushed - s) * crushAmt;
            };
            auto compBus = [&] (float s)
            {
                if (compMixNow >= 0.999f)
                    return holdLevel (s);
                if (compMixNow > 0.0008f)
                    return s + (holdLevel (s) - s) * compMixNow;
                return s;
            };
            auto tapeBus = [&] (float s)
            {
                return tapeStage (s, slowBus);
            };
            auto filtBus = [&] (float s)
            {
                if (filtType <= 0)
                    return s;
                if (filtMixNow >= 0.999f)
                    return applyFilter (s, filtType);
                if (filtMixNow > 0.0008f)
                    return s + (applyFilter (s, filtType) - s) * filtMixNow;
                return s;
            };
            auto pushDelay = [&] (float into)
            {
                into = std::clamp (into, -2.f, 2.f);
                delay[(size_t) write] = into;
                write += 1;
                if (write >= delayLen)
                    write = 0;
            };
            auto stamp = [&] (float)
            {
                samRing[scopeWrite % (uint32_t) scopeN] = samHeard;
                ++scopeWrite;
            };

            // A single note is left alone. Extra sounding notes share the bus,
            // eased over a short glide, so a held chord stays under the crush
            // clip and the output ceiling.
            int sounding = 0;
            for (const auto& voice : voices)
                if (voice.active)
                    ++sounding;
            const float polyTarget = sounding <= 1
                ? 1.f
                : 1.f / (1.f + 0.72f * (float) (sounding - 1));
            const float polyDelta = polyTarget - polyGain;
            if (std::abs (polyDelta) <= 1.0e-6f)
                polyGain = polyTarget;
            else
                polyGain += polyDelta * polyCoef;
            x *= polyGain;

            // Stock chain: the tape runs first, on the whole voice mix, sub
            // included. Order 5 keeps it after the delay instead.
            if (stockRoute && fxOrder != 5)
                x = tapeBus (x);

            float body = x;
            if (! stockRoute)
            {
                if (filtType == 0)
                {
                    filtIc1 = 0.f;
                    filtIc2 = 0.f;
                }
                glideDelay (targetDelay);
                const float y = readDelay (smoothedDelay);
                float sig = x;
                float send = x;
                for (int n = 0; n < 6; ++n)
                {
                    switch (order[n])
                    {
                        case 0: sig = tapeBus (sig); break;
                        case 1: sig = crushBus (sig); break;
                        case 2: sig = filtBus (sig); break;
                        case 3:
                            send = sig;
                            sig = send * dryGain + y;
                            break;
                        case 4: sig = applyReverb (sig); break;
                        default: sig = compBus (sig); break;
                    }
                }
                pushDelay (send * wetGain + y * feedNow);
                stamp (send);
                body = sig;
            }
            else if (fxOrder == 0)
            {
                if (filtType == 0)
                {
                    filtIc1 = 0.f;
                    filtIc2 = 0.f;
                }
                else if (filtPlace == 0)
                    x = filtBus (x);

                stamp (x);

                glideDelay (targetDelay);
                const float y = readDelay (smoothedDelay);
                float mixed = x * dryGain + y;
                if (filtType > 0 && filtPlace == 1)
                    mixed = filtBus (mixed);
                // Tone filters once, then the echo copies it. Out filters the mix,
                // and the repeats pass through that filter again.
                pushDelay (x * wetGain + (filtPlace == 1 ? mixed : y) * feedNow);

                body = compBus (applyReverb (mixed));
            }
            else
            {
                if (filtType == 0)
                {
                    filtIc1 = 0.f;
                    filtIc2 = 0.f;
                }
                glideDelay (targetDelay);
                const float y = readDelay (smoothedDelay);
                float mixed = x;
                if (fxOrder == 1)
                {
                    x = crushBus (filtBus (x));
                    stamp (x);
                    mixed = x * dryGain + y;
                    pushDelay (x * wetGain + y * feedNow);
                    body = compBus (applyReverb (mixed));
                }
                else if (fxOrder == 2)
                {
                    x = filtBus (x);
                    stamp (x);
                    mixed = crushBus (x * dryGain + y);
                    pushDelay (x * wetGain + y * feedNow);
                    body = compBus (applyReverb (mixed));
                }
                else if (fxOrder == 3)
                {
                    x = compBus (applyReverb (filtBus (x)));
                    stamp (x);
                    mixed = x * dryGain + y;
                    pushDelay (x * wetGain + y * feedNow);
                    body = mixed;
                }
                else if (fxOrder == 4)
                {
                    stamp (x);
                    mixed = filtBus (x * dryGain + y);
                    pushDelay (x * wetGain + mixed * feedNow);
                    body = compBus (applyReverb (mixed));
                }
                else
                {
                    x = filtBus (x);
                    stamp (x);
                    mixed = tapeBus (x * dryGain + y);
                    pushDelay (x * wetGain + y * feedNow);
                    body = compBus (applyReverb (mixed));
                }
            }
            const float out = settleBus (applyLimit (body * level));
            scopeRing[(scopeWrite - 1u) % (uint32_t) scopeN] = out;
            for (int c = 0; c < numChannels; ++c)
                outputs[c][i] = out;
        }

        for (int n = 0; n < ClickParams::lfoCount; ++n)
            lfoVis[(size_t) n].store (lfoOut[n], std::memory_order_relaxed);

        mix.filterHz = lfoBaseCut;
        mix.filterQ = lfoBaseQ;
        mix.delaySec = lfoBaseDelay;
        mix.echo = lfoBaseEcho;
        mix.crushMix = lfoBaseWet;
        mix.glitch = lfoBaseGlitch;
        mix.slow = lfoBaseSlow;
        mix.level = lfoBaseLevel;
        mix.bits = lfoBaseBits;
        mix.hold = lfoBaseHold;
        mix.feedback = lfoBaseFb;
        mix.scatter = lfoBaseScatter;
        mix.doublet = 0.f;
        mix.width = lfoBaseWidth;
        mix.samDrag = lfoBaseSamDrag;
        mix.subTune = lfoBaseTune;
        mix.subMix = lfoBaseSub;
        mix.formShift = lfoBaseShift;
        mix.formSpread = lfoBaseSpread;
        mix.formBreath = lfoBaseBreath;
        mix.resoDamp = lfoBaseDamp;
        mix.resoTune = lfoBaseRTune;
        mix.resoStack = lfoBaseStack;
        mix.attack = lfoBaseAttack;
        mix.decay = lfoBaseDecay;
        mix.sustain = lfoBaseSustain;
        mix.release = lfoBaseRelease;
        mix.fAttack = lfoBaseFAttack;
        mix.fDecay = lfoBaseFDecay;
        mix.fSustain = lfoBaseFSustain;
        mix.fRelease = lfoBaseFRelease;
        mix.filtAmt = lfoBaseFiltAmt;
        mix.seed = lfoBaseSeed;
        mix.steps = lfoBaseSteps;
        samPitchMul = 1.f;
        samSpeedMul = 1.f;

        scopePub.store (scopeWrite, std::memory_order_release);
    }

    int activeVoices() const
    {
        int n = 0;
        for (const auto& voice : voices)
            if (voice.active)
                ++n;
        return n;
    }

    // The output, after the limiter. This is the trace on the logo.
    int copyScope (float* dst, int maxN) const { return copyRing (scopeRing, dst, maxN); }
    // Sam's voice only. Other waves leave this silent.
    int copySamScope (float* dst, int maxN) const { return copyRing (samRing, dst, maxN); }

    float bassHz() const { return scopeHz.load (std::memory_order_acquire); }
    float lfoAmount (int index) const
    {
        if (index < 0 || index >= ClickParams::lfoCount)
            return 0.f;
        return lfoVis[(size_t) index].load (std::memory_order_relaxed);
    }
    double engineSampleRate() const { return sampleRate; }

private:
    struct Rng
    {
        uint32_t s;
        explicit Rng (uint32_t seed) : s (seed == 0 ? 1u : seed) {}

        float next()
        {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            if (s == 0)
                s = 0xA5A5u;
            return (s >> 8) * (1.0f / 16777216.0f);
        }

        int irand (int n)
        {
            if (n <= 1)
                return 0;
            return (int) (next() * (float) n);
        }
    };

    struct Layer
    {
        std::vector<float> buf;
        int len = 0;
        double read = 0;
        float rate = 1.f;
        float pitch = 1.f;
        double hop = 0;
        double grain = 512;
        bool speech = false;
        int delay = 0;
        bool skip = false;
        int skipDelay = 0;
        double skipRead = 0;
        double skipHop = 0;
        float skipRate = 1.f;
        float skipGain = 0.2f;
        bool playing = false;
        uint32_t noise = 0;
        float noteHz = 0.f;
        float speechRatio = 1.f;
        int speechSlotSeen = -1;
        int fadeSlot = -1;
        int fadeLeft = 0;
        int fadeSpan = 1;
        int fadeLen = 0;
        double fadeRead = 0;
        double fadeHop = 0;
        float fadePitch = 1.f;
        double loopFrom = 0;
        double loopHop = 0;
        int loopLeft = 0;
        int loopSpan = 1;
    };

    struct Voice
    {
        Layer layer[2];
        int nLayers = 0;
        float gain = 0.f;
        bool active = false;
        bool held = false;
        int note = -1;
        int stage = 0;
        float env = 0.f;
        int subStage = 0;
        float subEnv = 0.f;
        int holdLeft = 0;
        float crushed = 0.f;
        int muteLeft = 0;
        float stutter = 0.f;
        float lastOut = 0.f;
        float fadeFrom = 0.f;
        int fadeLeft = 0;
        int fadeSpan = 1;
        uint32_t hash = 1;
        uint32_t born = 0;
        double phase = 0;
        double phase2 = 0;
        double subPhase = 0;
        uint32_t oscNoise = 1;
        int beatCell = -100000;
        int scatStep = -100000;
        int scatSamples = 0;
        float scatMul = 1.f;
        float scatTarget = 1.f;
        float playHz = 0.f;
        uint32_t scatRng = 1;
    };

    static float midiHz (int note)
    {
        return 440.f * std::pow (2.f, ((float) note - 69.f) / 12.f);
    }

    static float lerp (float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    static float readWrapped (const float* buf, int len, double pos)
    {
        if (len < 2)
            return 0.f;
        const double span = (double) len;
        while (pos < 0.0)
            pos += span;
        while (pos >= span)
            pos -= span;
        const int i = (int) pos;
        const int j = (i + 1 >= len) ? 0 : i + 1;
        const float f = (float) (pos - i);
        return buf[i] * (1.f - f) + buf[j] * f;
    }

    static float drawNoise (uint32_t& s)
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        if (s == 0)
            s = 0xA5A5u;
        return (s >> 8) * (1.0f / 16777216.0f) * 2.f - 1.f;
    }

    Voice* allocate()
    {
        Voice* oldest = &voices[0];
        for (auto& voice : voices)
        {
            if (! voice.active)
                return &voice;
            if (voice.born < oldest->born)
                oldest = &voice;
        }
        oldest->active = false;
        return oldest;
    }

    // Lowest note still sounding. The scope locks its sweep to this.
    void noteBass()
    {
        int lowest = 999;
        float hz = 0.f;
        for (const auto& voice : voices)
        {
            if (! voice.active || voice.note < 0 || voice.env <= 0.0008f)
                continue;
            if (voice.note < lowest)
            {
                lowest = voice.note;
                const float slowNow = std::clamp (mix.slow, 0.05f, 4.f);
                hz = midiHz (voice.note) * slowNow * bendMul;
            }
        }
        scopeHz.store (hz, std::memory_order_relaxed);
    }

    // Steady tone. Kept under 0.5 so degrade's 1.8x gain does not clip the
    // slope flat — that clip is why bits vanish on a full-scale square.
    static float waveAt (int kind, double phase, float width, uint32_t& noise)
    {
        const double p = phase - std::floor (phase);
        const double w = (double) std::clamp (width, 0.05f, 0.95f);
        if (std::abs (w - 0.5) <= 1.0e-4)
        {
            switch (kind)
            {
                case 1: return std::sin ((float) (p * 6.28318530718));
                case 2: return (float) (2.0 * p - 1.0);
                case 8: return (float) (1.0 - 2.0 * p);
                case 3: return p < 0.5 ? 1.f : -1.f;
                case 4: return p < 0.5 ? (float) (4.0 * p - 1.0) : (float) (3.0 - 4.0 * p);
                default: return drawNoise (noise);
            }
        }
        switch (kind)
        {
            case 1:
            {
                const double t = p < w ? 0.5 * (p / w)
                                       : 0.5 + 0.5 * ((p - w) / (1.0 - w));
                return std::sin ((float) (t * 6.28318530718));
            }
            case 2:
            {
                const double curve = std::log (0.5) / std::log (w);
                return (float) (2.0 * std::pow (std::max (p, 1.0e-12), curve) - 1.0);
            }
            case 8:
            {
                const double curve = std::log (0.5) / std::log (w);
                return (float) (1.0 - 2.0 * std::pow (std::max (p, 1.0e-12), curve));
            }
            case 3: return p < w ? 1.f : -1.f;
            case 4:
                if (p < w)
                    return (float) (p / w * 2.0 - 1.0);
                return (float) (1.0 - (p - w) / (1.0 - w) * 2.0);
            default: return drawNoise (noise);
        }
    }

    // Second oscillator, tuned in semitones from the note. FM uses it as a
    // rate bend. The other modes mix the sample in shapeSub().
    float subOsc (Voice& voice, float slowNow)
    {
        // Menu order is sine, square, saw up, saw down, triangle.
        // Oscillator cases are sine, saw up, square, triangle, and 8 for saw down.
        static const int shape[5] = { 1, 3, 2, 8, 4 };
        const int kind = shape[std::clamp (mix.subWave, 0, 4)];
        const float semi = std::clamp (mix.subTune + mix.subFine / 100.f, -36.f, 36.f);
        const float ratio = std::pow (2.f, semi / 12.f);
        const double inc = (double) midiHz (voice.note) * (double) slowNow * (double) ratio
                         * (double) bendMul / sampleRate;
        uint32_t unused = 1;
        const float s = waveAt (kind, voice.subPhase, mix.subWidth, unused);
        voice.subPhase += inc;
        voice.subPhase -= std::floor (voice.subPhase);
        return s;
    }

    static float shapeSub (float s, float sub, int mode, float depth)
    {
        const float m = sub * 0.5f;
        float out = s;
        switch (mode)
        {
            case 1: out = s + m * depth; break;
            case 2: out = s - m * depth; break;
            case 3: out = s * (1.f - depth + depth * sub); break;
            case 4:
            {
                const float denom = 1.f - sub * depth * 0.85f;
                const float mag = std::max (0.15f, std::abs (denom));
                out = s / std::copysign (mag, denom);
                break;
            }
            case 5: out = s * (1.f + depth * sub) / (1.f + depth); break;
            case 7: out = s + (std::min (s, m) - s) * depth; break;
            case 8: out = s + (std::max (s, m) - s) * depth; break;
            default: break;
        }
        return std::clamp (out, -1.5f, 1.5f);
    }

    float sourceHz (Voice& voice)
    {
        if (voice.note < 0)
            return 440.f * bendMul;
        const float target = midiHz (voice.note) * bendMul;
        if (mix.srcGlide < 0.001f)
        {
            voice.playHz = target;
            return target;
        }
        if (voice.playHz < 20.f)
            voice.playHz = target;
        const float coef = 1.f / std::max (1.f, mix.srcGlide * (float) sampleRate);
        voice.playHz += coef * (target - voice.playHz);
        return voice.playHz;
    }

    float oscillator (Voice& voice, float slowNow, float warble)
    {
        const int kind = mix.wave == 8 ? 8 : std::clamp (mix.wave, 1, 5);
        const double inc = (double) sourceHz (voice) * (double) slowNow * (double) warble / sampleRate;
        float s = waveAt (kind, voice.phase, mix.width, voice.oscNoise) * 0.5f;
        voice.phase += inc;
        voice.phase -= std::floor (voice.phase);
        return s;
    }


    // Read the loop.
    float sampleLayer (Layer& layer, float play)
    {
        if (! layer.playing || layer.len < 2)
            return 0.f;
        const double span = (double) layer.len;
        auto pull = [&] (double& read, float rate) -> float
        {
            if (read < 0.0)
                read = 0.0;
            while (read >= span)
                read -= span;
            const float s = readWrapped (layer.buf.data(), layer.len, read);
            read += (double) rate * (double) bendMul;
            while (read >= span)
                read -= span;
            return s;
        };

        float s = 0.f;
        if (layer.delay > 0)
            --layer.delay;
        else
        {
            s += pull (layer.read, play);
            if (layer.noise != 0)
                s += drawNoise (layer.noise) * 0.7f;
        }
        return s;
    }

    // Speech keeps the phrase clock separate from the pitch, so a higher
    // key does not make Sam talk faster. The word itself is the shared
    // locked sample, so a new render is heard on a note that is still held.
    float sampleSpeech (Voice& voice, Layer& layer, float slowNow, float rateMul, float wander)
    {
        const int slot = speechSlot.load (std::memory_order_acquire);
        const SpeechLoop& loop = speech[slot];
        if (! layer.playing || loop.len < 2 || loop.samples.empty())
            return 0.f;

        if (layer.speechSlotSeen != slot)
        {
            const double oldLen = layer.len > 1 ? (double) layer.len : (double) loop.len;
            const double frac = layer.speechSlotSeen < 0 ? 0.0 : layer.read / oldLen;
            const double skipFrac = layer.speechSlotSeen < 0 ? 0.0 : layer.skipRead / oldLen;
            if (layer.speechSlotSeen >= 0 && layer.len > 1)
            {
                layer.fadeSlot = layer.speechSlotSeen;
                layer.fadeLen = layer.len;
                layer.fadeRead = layer.read;
                layer.fadeHop = layer.hop;
                layer.fadePitch = layer.speechRatio;
                layer.fadeSpan = std::max (1, (int) std::lround (sampleRate * 0.03));
                layer.fadeLeft = layer.fadeSpan;
            }
            layer.speechSlotSeen = slot;
            layer.len = loop.len;
            layer.read = frac * (double) loop.len;
            layer.skipRead = skipFrac * (double) loop.len;
            layer.rate = (float) (loop.renderedRate / sampleRate);
            if (loop.refHz > 20.f)
                layer.pitch = sourceHz (voice) / loop.refHz;
        }
        else if (loop.refHz > 20.f)
            layer.pitch = sourceHz (voice) / loop.refHz;

        float clock = slowNow;
        if (mix.sync && mix.restartQuarters > 0.02f && loop.renderedRate > 1000.0 && clockBpm > 1.0)
        {
            const double phraseSec = (double) loop.len / loop.renderedRate;
            const double divSec = (double) mix.restartQuarters * 60.0 / clockBpm;
            if (phraseSec > 0.001 && divSec > 0.001)
                clock = (float) std::clamp (phraseSec / divSec, 0.05, 32.0);
        }
        const float pitchScale = mix.pitchFix ? 1.f : clock;
        const float talk = clock * samSpeedMul * std::clamp (mix.samDrag, 0.f, 1.f);
        const float targetRatio = std::clamp (layer.pitch * rateMul * samPitchMul * wander * pitchScale, 0.25f, 4.f);
        const float ratioSlew = 1.f / std::max (1.f, 0.01f * (float) sampleRate);
        layer.speechRatio += ratioSlew * (targetRatio - layer.speechRatio);
        const float pitched = layer.speechRatio;
        const float basePitch = std::max (0.05f, layer.pitch * rateMul * samPitchMul * pitchScale);
        const float slewedWander = std::clamp (pitched / basePitch, 0.25f, 4.f);
        const float timeRate = std::max (0.f, layer.rate * talk * slewedWander);
        const double grain = layer.grain > 64.0 ? layer.grain : 512.0;
        const float* samples = loop.samples.data();
        const int len = loop.len;

        // Two grains, half a window apart. Each one is silent at the moment
        // its read head jumps, and the other is at full level, so the splice
        // does not click.
        auto grainPair = [&] (const float* buf, int bufLen, double g, double& read, double& hop,
                              float ratio, float time) -> float
        {
            if (bufLen < 2 || g < 8.0)
                return 0.f;
            const double span = (double) bufLen;
            auto wrap = [&] (double p)
            {
                p = std::fmod (p, span);
                if (p < 0.0)
                    p += span;
                return p;
            };
            auto window = [&] (double h)
            {
                return 0.5f * (1.f - std::cos (6.28318530718f * (float) (h / g)));
            };
            const double hop2 = hop + g * 0.5 >= g ? hop + g * 0.5 - g : hop + g * 0.5;
            const double read2 = wrap (read + (double) (ratio - time) * (hop2 - hop));
            const float out = readWrapped (buf, bufLen, wrap (read)) * window (hop)
                            + readWrapped (buf, bufLen, read2) * window (hop2);
            read += (double) ratio;
            hop += 1.0;
            if (hop >= g)
            {
                hop -= g;
                read -= (double) (ratio - time) * g;
            }
            read = wrap (read);
            if (hop < 0.0)
                hop = 0.0;
            return out;
        };

        float s = 0.f;
        if (layer.delay > 0)
            --layer.delay;
        else
            s = grainPair (samples, len, grain, layer.read, layer.hop, pitched, timeRate);

        if (layer.loopLeft > 0 && layer.loopSpan > 0)
        {
            const float old = grainPair (samples, len, grain, layer.loopFrom, layer.loopHop, pitched, timeRate);
            const float u = (float) layer.loopLeft / (float) layer.loopSpan;
            const float w = 0.5f * (1.f - std::cos (3.14159265f * (1.f - u)));
            s = old * (1.f - w) + s * w;
            --layer.loopLeft;
        }

        if (layer.fadeLeft > 0 && layer.fadeSlot >= 0 && layer.fadeSlot < 2
            && layer.fadeLen > 1 && ! speech[layer.fadeSlot].samples.empty())
        {
            const float* oldBuf = speech[layer.fadeSlot].samples.data();
            const float old = grainPair (oldBuf, layer.fadeLen, grain, layer.fadeRead, layer.fadeHop,
                                         layer.fadePitch, timeRate);
            const float u = (float) layer.fadeLeft / (float) layer.fadeSpan;
            const float w = 0.5f * (1.f - std::cos (3.14159265f * (1.f - u)));
            s = s * (1.f - w) + old * w;
            --layer.fadeLeft;
        }
        return s;
    }

    // A deck that is not at play speed saturates a little. Rate 1 is untouched.
    // The bend stays at the same loudness: slowing used to lift quiet notes.
    float tapeColour (float x, float rate) const
    {
        const float depart = std::abs (std::log (std::max (rate, 0.05f)));
        if (depart < 0.02f)
            return x;
        const float drive = 1.f + depart * 2.4f;
        const float bent = x + 0.18f * depart * x * std::abs (x);
        return std::tanh (bent * drive) / drive;
    }

    // The tape stage. Everything on the bus passes through a short delay
    // line whose read head sways, so the pitch of the whole mix sways with
    // it: wow at the warble rate, a lighter flutter about five times faster,
    // and a slow wander when scatterGlide is up. The sway is sized so the
    // pitch reaches the depth the amount knob asks for, within a 50 ms
    // head travel, so slow wow stays gentle and quick wow can go deep.
    // Depth 0 bypasses the line. Then the deck colour, when pitch follows speed.
    static uint32_t tapeHash (uint32_t s)
    {
        s ^= s >> 16;
        s *= 0x7FEB352Du;
        s ^= s >> 15;
        s *= 0x846CA68Bu;
        s ^= s >> 16;
        return s;
    }

    static float tapeUnit (uint32_t s)
    {
        return ((tapeHash (s) >> 8) / 16777216.f) * 2.f - 1.f;
    }

    void clearTape()
    {
        std::fill (tapeRing.begin(), tapeRing.end(), 0.f);
        tapeWrite = 0;
        tapeBase = 0.f;
        tapeDepth = 0.f;
        tapeWowPhase = 0.f;
        tapeFlutPhase = 0.f;
        tapeDriftPhase = 0.f;
        tapeDriftStep = -1;
        tapeDriftFrom = 0.f;
        tapeDriftTo = 0.f;
    }

    // Once per block: rates and shares from the knobs.
    void tuneTape()
    {
        const float sr = (float) std::max (1.0, sampleRate);
        tapeRate = mix.warbleHz > 0.05f ? std::clamp (mix.warbleHz, 0.05f, 40.f) : 1.2f;
        tapeFlutAmp = 0.18f * std::clamp ((12.f - tapeRate) / 8.f, 0.f, 1.f);
        const float glide = std::clamp (mix.scatterGlide, 0.f, 4.f);
        tapeWanderSec = std::max (0.1f, glide);
        tapeWander = glide < 0.001f ? 0.f : 0.5f * std::clamp (glide / 0.1f, 0.f, 1.f);
        tapeWowInc = tapeRate / sr;
        tapeFlutInc = tapeRate * 5.3f / sr;
        tapeDriftInc = 1.f / (tapeWanderSec * sr);
        tapeSeedHash = (uint32_t) std::lround (std::clamp (mix.seed, 0.f, 999.f) * 17.f);
        tapeSeedPhase = (tapeHash (tapeSeedHash ^ 0x5bd1e995u) >> 8) / 16777216.f;
        tapeCoef = 1.f - std::exp (-1.f / std::max (1.f, 0.08f * sr));
        tapeOn = mix.scatMode == 0;
    }

    float tapeStage (float x, float rate)
    {
        if (tapeLen > 16)
        {
            tapeRing[(size_t) tapeWrite] = x;
            const int wrote = tapeWrite;
            tapeWrite = tapeWrite + 1 >= tapeLen ? 0 : tapeWrite + 1;

            const float depth = tapeOn ? std::clamp (mix.scatter, 0.f, 1.f) : 0.f;
            float depthTarget = 0.f;
            float baseTarget = 0.f;
            if (depth > 0.0008f)
            {
                const float semis = 6.f * depth * depth;
                const float reach = std::exp2 (semis / 12.f) - 1.f;
                const float wave = (1.f - tapeWander) * 6.2831853f * tapeRate * (1.f + 5.3f * tapeFlutAmp);
                const float wander = tapeWander * 3.14159265f / tapeWanderSec;
                const float sway = std::min (0.05f, reach / std::max (0.01f, wave + wander));
                depthTarget = sway * (float) sampleRate;
                baseTarget = depthTarget * (1.f + tapeFlutAmp) + 3.f;
            }
            tapeDepth += (depthTarget - tapeDepth) * tapeCoef;
            tapeBase += (baseTarget - tapeBase) * tapeCoef;

            if (baseTarget > 0.f || tapeBase > 0.02f)
            {
                tapeWowPhase += tapeWowInc;
                tapeWowPhase -= std::floor (tapeWowPhase);
                tapeFlutPhase += tapeFlutInc;
                tapeFlutPhase -= std::floor (tapeFlutPhase);
                if (tapeDriftStep < 0)
                {
                    tapeDriftStep = 0;
                    tapeDriftFrom = tapeUnit (tapeSeedHash ^ 0u);
                    tapeDriftTo = tapeUnit (tapeSeedHash ^ 0x9E3779B9u);
                    tapeDriftPhase = 0.f;
                }
                tapeDriftPhase += tapeDriftInc;
                if (tapeDriftPhase >= 1.f)
                {
                    tapeDriftPhase -= std::floor (tapeDriftPhase);
                    ++tapeDriftStep;
                    tapeDriftFrom = tapeDriftTo;
                    tapeDriftTo = tapeUnit (tapeSeedHash ^ ((uint32_t) (tapeDriftStep + 1) * 0x9E3779B9u));
                }
                const float wow = std::sin (6.2831853f * (tapeWowPhase + tapeSeedPhase));
                const float flutter = tapeFlutAmp * std::sin (6.2831853f * (tapeFlutPhase + tapeSeedPhase * 0.37f));
                const float ease = 0.5f - 0.5f * std::cos (3.14159265f * tapeDriftPhase);
                const float drift = tapeDriftFrom + (tapeDriftTo - tapeDriftFrom) * ease;
                const float m = (wow + flutter) * (1.f - tapeWander) + drift * tapeWander;

                const float d = std::clamp (tapeBase + tapeDepth * m, 2.f, (float) tapeLen - 4.f);
                float pos = (float) wrote - d;
                if (pos < 0.f)
                    pos += (float) tapeLen;
                int i1 = (int) pos;
                const float f = pos - (float) i1;
                if (i1 >= tapeLen)
                    i1 -= tapeLen;
                const int i0 = i1 == 0 ? tapeLen - 1 : i1 - 1;
                const int i2 = i1 + 1 >= tapeLen ? i1 + 1 - tapeLen : i1 + 1;
                const int i3 = i2 + 1 >= tapeLen ? i2 + 1 - tapeLen : i2 + 1;
                const float y0 = tapeRing[(size_t) i0];
                const float y1 = tapeRing[(size_t) i1];
                const float y2 = tapeRing[(size_t) i2];
                const float y3 = tapeRing[(size_t) i3];
                const float c1 = 0.5f * (y2 - y0);
                const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
                const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                x = ((c3 * f + c2) * f + c1) * f + y1;
            }
        }
        if (mix.pitchFix)
            return x;
        return tapeColour (x, rate);
    }

    struct CrushMem
    {
        int holdLeft = 0;
        float crushed = 0.f;
        int muteLeft = 0;
        float stutter = 0.f;
        uint32_t hash = 1;
    };

    // Bit crush, then sample-hold, then random dropouts.
    // bits  sets how many volume steps (2 is coarse, 16 is clean).
    // hold  is how many samples that step stays frozen.
    // glitch lengthens hold and sometimes outputs silence or a stuck sample.
    // A square blip is already two levels, so bits barely move it.
    // A chit, or any sloped wave, shows the steps.
    template <typename Mem>
    float degradeAny (Mem& voice, float sample)
    {
        const int bits = mix.bitsOn ? std::clamp ((int) std::lround (mix.bits), 2, 16) : 16;
        const float steps = (float) ((1u << bits) - 1u);
        int hold = mix.crushOn ? std::clamp ((int) std::lround (mix.hold), 1, 80) : 1;
        const float glitch = mix.crushOn ? std::clamp (mix.glitch, 0.f, 1.f) : 0.f;
        hold += (int) std::lround (glitch * 24.f);
        if (voice.holdLeft <= 0)
        {
            voice.holdLeft = std::max (1, hold);
            const float clipped = std::clamp (sample * 1.8f, -1.f, 1.f);
            const float q = std::floor ((clipped * 0.5f + 0.5f) * steps + 0.5f) / steps;
            voice.crushed = q * 2.f - 1.f;
        }
        --voice.holdLeft;

        float out = voice.crushed;
        if (glitch > 0.001f)
        {
            voice.hash = voice.hash * 1664525u + 1013904223u;
            if (voice.hash == 0)
                voice.hash = 1;
            const float roll = (voice.hash >> 8) * (1.f / 16777216.f);
            if (voice.muteLeft <= 0 && roll < glitch * 0.08f)
                voice.muteLeft = 4 + (int) (glitch * 36.f);
            if (voice.muteLeft > 0)
            {
                --voice.muteLeft;
                out = (voice.hash & 2u) ? 0.f : voice.stutter;
            }
            else
            {
                voice.stutter = out;
            }
        }
        return out;
    }

    float degrade (Voice& voice, float sample)
    {
        return degradeAny (voice, sample);
    }

    void followTone()
    {
        if (mix.poly <= 0)
        {
            Voice* keep = nullptr;
            for (auto& voice : voices)
            {
                if (voice.active && voice.held && (keep == nullptr || voice.born > keep->born))
                    keep = &voice;
            }
            for (auto& voice : voices)
            {
                if (voice.active && voice.held && &voice != keep)
                    letGo (voice);
            }
        }

        const int stamp = mix.wave * 10000000
            + mix.voice * 1000000
            + (int) std::lround (std::clamp (mix.scatter, 0.f, 1.f) * 200.f)
            + (int) std::lround (std::clamp (mix.seed, 0.f, 999.f)) * 1000
            + mix.scatMode * 500000
            + (int) std::lround (std::clamp (mix.steps, 1.f, 16.f)) * 100;
        if (shapeStamp < 0)
        {
            shapeStamp = stamp;
            return;
        }
        if (stamp == shapeStamp)
            return;
        shapeStamp = stamp;

        for (auto& voice : voices)
        {
            if (! voice.active || voice.note < 0)
                continue;
            const float env = voice.env;
            const int stage = voice.stage;
            const float subEnv = voice.subEnv;
            const int subStage = voice.subStage;
            const bool held = voice.held;
            const float gain = voice.gain;
            const uint32_t born = voice.born;
            const int note = voice.note;
            trigger (&voice, note, 1.f, mix);
            voice.env = env;
            voice.stage = stage;
            voice.subEnv = subEnv;
            voice.subStage = subStage;
            voice.held = held;
            voice.gain = gain;
            voice.born = born;
        }
    }

    // Glide a control toward its target. A value that is already there stays exact.
    float chase (float& state, float target) const
    {
        const float d = target - state;
        if (std::abs (d) <= 1.0e-6f)
        {
            state = target;
            return target;
        }
        state += d * slewCoef;
        return state;
    }

    // The read head never jumps by more than one sample, so a delay move does not click.
    void glideDelay (float target)
    {
        float delta = 0.002f * (target - smoothedDelay);
        if (delta > 1.f)
            delta = 1.f;
        else if (delta < -1.f)
            delta = -1.f;
        smoothedDelay += delta;
    }

    float settleVoice (Voice& voice, float s)
    {
        if (voice.fadeLeft > 0 && voice.fadeSpan > 1)
        {
            const float t = 1.f - (float) (voice.fadeLeft - 1) / (float) (voice.fadeSpan - 1);
            const float w = 0.5f * (1.f - std::cos (3.14159265f * std::clamp (t, 0.f, 1.f)));
            s = voice.fadeFrom * (1.f - w) + s * w;
            --voice.fadeLeft;
        }
        voice.lastOut = s;
        return s;
    }

    float settleBus (float s)
    {
        if (busFadeLeft > 0 && busFadeSpan > 1)
        {
            const float t = 1.f - (float) (busFadeLeft - 1) / (float) (busFadeSpan - 1);
            const float w = 0.5f * (1.f - std::cos (3.14159265f * std::clamp (t, 0.f, 1.f)));
            s = busFadeFrom * (1.f - w) + s * w;
            --busFadeLeft;
        }
        busLast = s;
        return s;
    }

    // stage 0 attack, 1 decay, 2 sustain while the key is down, 3 release.
    float advanceEnv (Voice& voice)
    {
        const float sustain = std::clamp (mix.sustain, 0.f, 1.f);
        if (voice.stage == 0)
        {
            const float step = 1.f / std::max (1.f, mix.attack * (float) sampleRate);
            voice.env += step;
            if (voice.env >= 1.f)
            {
                voice.env = 1.f;
                voice.stage = 1;
            }
        }
        else if (voice.stage == 1)
        {
            const float step = (1.f - sustain) / std::max (1.f, mix.decay * (float) sampleRate);
            voice.env -= step;
            if (voice.env <= sustain)
            {
                voice.env = sustain;
                voice.stage = 2;
            }
        }
        else if (voice.stage == 2)
        {
            voice.env = chase (voice.env, sustain);
        }
        else
        {
            const float rel = std::max (mix.release, 0.003f);
            const float step = 1.f / std::max (1.f, rel * (float) sampleRate);
            voice.env -= step;
            if (voice.env <= 0.0008f)
            {
                voice.env = 0.f;
                voice.active = false;
            }
        }
        return std::max (0.f, voice.env);
    }

    // The sub's own attack / decay / sustain / release. It does not end the voice.
    float advanceSubEnv (Voice& voice)
    {
        const float sustain = std::clamp (mix.subSustain, 0.f, 1.f);
        if (voice.subStage == 0)
        {
            const float step = 1.f / std::max (1.f, mix.subAttack * (float) sampleRate);
            voice.subEnv += step;
            if (voice.subEnv >= 1.f)
            {
                voice.subEnv = 1.f;
                voice.subStage = 1;
            }
        }
        else if (voice.subStage == 1)
        {
            const float step = (1.f - sustain) / std::max (1.f, mix.subDecay * (float) sampleRate);
            voice.subEnv -= step;
            if (voice.subEnv <= sustain)
            {
                voice.subEnv = sustain;
                voice.subStage = 2;
            }
        }
        else if (voice.subStage == 2)
        {
            voice.subEnv = chase (voice.subEnv, sustain);
        }
        else
        {
            const float rel = std::max (mix.subRelease, 0.003f);
            const float step = 1.f / std::max (1.f, rel * (float) sampleRate);
            voice.subEnv -= step;
            if (voice.subEnv <= 0.0008f)
                voice.subEnv = 0.f;
        }
        return std::max (0.f, voice.subEnv);
    }

    void pushMono (int note, float velocity)
    {
        int w = 0;
        for (int i = 0; i < monoN; ++i)
        {
            if (monoNote[i] == note)
                continue;
            monoNote[w] = monoNote[i];
            monoVel[w] = monoVel[i];
            ++w;
        }
        if (w < 16)
        {
            monoNote[w] = note;
            monoVel[w] = velocity;
            ++w;
        }
        monoN = w;
    }

    // Resonant filter. Bandpass and notch stay near unity in the pass band.
    // Formant is three vowel peaks. Resonator is a tuned ring.
    float applyFilter (float x, int type)
    {
        if (type == 5)
            return shapeFormant (x);
        if (type == 6)
            return shapeResonator (x);
        return shapeFilter (x, type);
    }

    float shapeFormant (float x)
    {
        const float span = std::log (2500.f) - std::log (80.f);
        const float u = std::clamp ((std::log (std::max (80.f, smoothedCut)) - std::log (80.f)) / span, 0.f, 1.f);
        const float vowel[4][3] = {
            { 300.f, 870.f, 2240.f },
            { 730.f, 1090.f, 2440.f },
            { 530.f, 1840.f, 2480.f },
            { 270.f, 2290.f, 3010.f },
        };
        const float pos = u * 3.f;
        const int a = std::min (2, (int) pos);
        const float t = pos - (float) a;
        const float q = std::clamp (smoothedQ, 0.5f, 8.f);
        const float k = 1.f / q;
        const float gain[3] = { 1.f, 0.62f, 0.38f };
        const float shift = std::clamp (mix.formShift, 0.5f, 2.f);
        const float spread = std::clamp (mix.formSpread, 0.25f, 2.5f);
        float peak[3];
        for (int i = 0; i < 3; ++i)
            peak[i] = (vowel[a][i] * (1.f - t) + vowel[a + 1][i] * t) * shift;
        const float root = peak[0];
        peak[1] = root + (peak[1] - root) * spread;
        peak[2] = root + (peak[2] - root) * spread;
        float excite = x;
        const float breath = std::clamp (mix.formBreath, 0.f, 1.f);
        if (breath > 0.001f)
        {
            formRng = formRng * 1664525u + 1013904223u;
            const float n = ((formRng >> 8) / 16777216.f) * 2.f - 1.f;
            excite += n * breath * 0.45f;
        }
        float y = 0.f;
        for (int i = 0; i < 3; ++i)
        {
            const float hz = std::clamp (peak[i], 40.f, (float) (sampleRate * 0.45));
            const float g = std::tan (3.14159265f * hz / (float) sampleRate);
            const float a1 = 1.f / (1.f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = excite - formIc2[i];
            const float v1 = a1 * formIc1[i] + a2 * v3;
            const float v2 = formIc2[i] + a2 * formIc1[i] + a3 * v3;
            formIc1[i] = 2.f * v1 - formIc1[i];
            formIc2[i] = 2.f * v2 - formIc2[i];
            y += k * v1 * gain[i];
        }
        y *= 0.9f;
        if (! std::isfinite (y))
        {
            formIc1[0] = formIc1[1] = formIc1[2] = 0.f;
            formIc2[0] = formIc2[1] = formIc2[2] = 0.f;
            return 0.f;
        }
        return std::clamp (y, -4.f, 4.f);
    }

    float shapeResonator (float x)
    {
        if (resoLen < 4)
            return x;
        const float semis = std::clamp (mix.resoTune, -24.f, 24.f);
        const float freq = std::clamp (smoothedCut * std::pow (2.f, semis / 12.f),
                                       40.f, (float) (sampleRate * 0.45));
        const float delaySamps = std::clamp ((float) sampleRate / freq, 2.f, (float) (resoLen - 3));
        const float fb = std::clamp (0.25f + (smoothedQ - 0.3f) * 0.09f, 0.2f, 0.96f);
        const int i = (int) delaySamps;
        const float frac = delaySamps - (float) i;
        auto at = [this] (int back)
        {
            int r = resoWrite - back;
            while (r < 0)
                r += resoLen;
            return reso[(size_t) r];
        };
        const float delayed = at (i) * (1.f - frac) + at (i + 1) * frac;
        float loop = delayed;
        const float damp = std::clamp (mix.resoDamp, 0.f, 1.f);
        if (damp > 0.0008f)
        {
            const float hz = std::clamp (7000.f * std::pow (0.06f, damp), 70.f, 8000.f);
            const float c = std::exp (-6.2831853f * hz / (float) sampleRate);
            resoDull = loop + c * (resoDull - loop);
            loop = delayed * (1.f - damp) + resoDull * damp;
        }
        float y = x * (1.f - fb) + loop * fb;
        const float stack = std::clamp (mix.resoStack, 0.f, 1.f);
        if (stack > 0.001f)
        {
            const float half = std::clamp (delaySamps * 0.5f, 2.f, (float) (resoLen - 3));
            const int hi = (int) half;
            const float hf = half - (float) hi;
            const float oct = at (hi) * (1.f - hf) + at (hi + 1) * hf;
            y += oct * fb * stack * 0.55f;
        }
        if (! std::isfinite (y))
        {
            std::fill (reso.begin(), reso.end(), 0.f);
            resoWrite = 0;
            return 0.f;
        }
        y = std::clamp (y, -4.f, 4.f);
        reso[(size_t) resoWrite] = y;
        if (++resoWrite >= resoLen)
            resoWrite = 0;
        return y;
    }

    float shapeFilter (float v0, int type)
    {
        const float g = std::tan (3.14159265f * smoothedCut / (float) sampleRate);
        const float k = 1.f / std::max (0.3f, smoothedQ);
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = v0 - filtIc2;
        const float v1 = a1 * filtIc1 + a2 * v3;
        const float v2 = filtIc2 + a2 * filtIc1 + a3 * v3;
        filtIc1 = 2.f * v1 - filtIc1;
        filtIc2 = 2.f * v2 - filtIc2;
        float y = v0;
        if (type == 1)      y = v2;
        else if (type == 2) y = v0 - k * v1 - v2;
        else if (type == 3) y = k * v1;
        else if (type == 4) y = v0 - k * v1;
        if (! std::isfinite (y))
        {
            filtIc1 = 0.f;
            filtIc2 = 0.f;
            return 0.f;
        }
        return std::clamp (y, -4.f, 4.f);
    }

    float readDelay (float samples)
    {
        if (delayLen < 4)
            return 0.f;
        float d = std::clamp (samples, 0.f, (float) (delayLen - 3));
        const int i = (int) d;
        const float f = d - (float) i;
        auto at = [this] (int back)
        {
            int r = write - back;
            while (r < 0)
                r += delayLen;
            return delay[(size_t) r];
        };
        if (i <= 0)
            return at (0);
        return at (i) * (1.f - f) + at (i + 1) * f;
    }

    // Copy the locked word. Talk rate stays put. The key only sets pitch.
    void addSpeech (Voice& voice, float hz)
    {
        if (voice.nLayers >= 2)
            return;
        const int slot = speechSlot.load (std::memory_order_acquire);
        const SpeechLoop& loop = speech[slot];
        if (loop.len < 2 || loop.refHz < 20.f)
            return;
        Layer& layer = voice.layer[voice.nLayers++];
        layer.len = loop.len;
        layer.rate = (float) (loop.renderedRate / sampleRate);
        layer.noteHz = hz;
        layer.pitch = hz / loop.refHz;
        layer.speechRatio = layer.pitch;
        layer.speech = true;
        layer.speechSlotSeen = -1;
        layer.fadeLeft = 0;
        layer.fadeSlot = -1;
        layer.loopLeft = 0;
        layer.grain = std::max (128.0, sampleRate * 0.018);
        layer.read = 0;
        layer.hop = 0;
        layer.skipRead = 0;
        layer.skipHop = 0;
        layer.delay = 0;
        layer.noise = 0;
        layer.playing = true;
    }

    // index 0 writes a blip. index 1 writes a chit. Both fit in one voice.
    void addLayer (Voice& voice, Rng& rng, int index,
                    const ClickParams& params, float target, bool late)
    {
        if (voice.nLayers >= 2)
            return;
        Layer& layer = voice.layer[voice.nLayers++];
        const bool chit = index == 1;
        const bool repeat = params.scatMode != 0;
        const float scatter = repeat ? 0.f : std::clamp (params.scatter, 0.f, 1.f);

        const float rate = std::clamp (
            lerp (1.f, 0.32f + rng.next() * 0.28f, scatter),
            0.05f, 4.f);
        const float dur = chit
            ? lerp (0.23f, 0.12f + rng.next() * 0.22f, scatter)
            : lerp (0.36f, 0.22f + rng.next() * 0.28f, scatter);
        const float duty = chit
            ? 0.12f
            : std::clamp (lerp (0.28f, 0.15f + rng.next() * 0.35f, scatter), 0.04f, 0.9f);

        int length = (int) std::floor (sampleRate * (double) std::clamp (dur, 0.05f, 0.5f));
        length = std::clamp (length, 8, cap - 2);
        const int maxLen = std::max (8, cap - 2);
        int stepSamps = 0;
        if (! repeat && params.sync && params.restartQuarters > 0.02f)
        {
            const float sec = params.restartQuarters * 60.f / (float) std::max (20.0, clockBpm);
            stepSamps = std::clamp ((int) std::lround ((double) sec * sampleRate), 32, std::max (32, maxLen / 2));
            length = std::clamp (stepSamps * 6, 8, maxLen);
        }

        layer.noise = 0;
        layer.speech = false;
        if (chit)
        {
            layer.len = renderChit (layer.buf.data(), stepSamps > 0 ? stepSamps : length, maxLen, rng, target, rate, duty, scatter);
            layer.noise = rng.s == 0 ? 1u : rng.s;
        }
        else
        {
            layer.len = renderBlip (layer.buf.data(), length, maxLen, rng, target, rate, duty, scatter, stepSamps);
        }

        layer.read = 0;
        layer.rate = rate;
        layer.delay = late ? (int) std::lround (0.06 * sampleRate) : 0;
        layer.skipRead = 0;
        layer.playing = true;
    }

    // Stepped pulse for the click. Steady waves are made in oscillator().
    // Each step is a whole number of cycles so the loop joins cleanly.
    int renderBlip (float* dst, int want, int maxLen, Rng& rng, float target, float rate, float duty, float scatter, int stepSamps)
    {
        const int steps = 4 + rng.irand (5);
        float raw[8];
        const int n = std::min (steps, 8);
        double logSum = 0;
        for (int s = 0; s < n; ++s)
        {
            raw[s] = 70.f + (float) rng.irand (22) *80.f;
            logSum += std::log ((double) raw[s]);
        }
        const float logMean = (float) (logSum / n);
        const float center = std::log (std::max (20.f, target / rate));
        for (int s = 0; s < n; ++s)
        {
            const float interval = std::log (raw[s]) - logMean;
            raw[s] = std::exp (center + interval * scatter);
            raw[s] = std::clamp (raw[s], 20.f, 16000.f);
        }

        const int share = stepSamps > 0 ? stepSamps : std::max (8, want / n);
        int pos = 0;
        for (int s = 0; s < n && pos + 2 <= maxLen; ++s)
        {
            const double period = sampleRate / (double) raw[s];
            const int room = maxLen - pos;
            int cycles = std::max (1, (int) std::lround ((double) share / period));
            auto lenFor = [&] (int c)
            {
                return std::max (1, (int) std::lround ((double) c * period));
            };
            while (cycles > 1 && lenFor (cycles) > room)
                --cycles;
            int len = lenFor (cycles);
            if (len > room || len < 2)
                break;
            for (int i = 0; i < len; ++i)
            {
                const double frac = (double) (((long long) i * cycles) % len) / (double) len;
                dst[pos++] = frac < (double) duty ? 0.85f : -0.85f;
            }
        }
        return std::max (2, pos);
    }

    // One pulse, no baked noise. sampleLayer adds fresh noise each sample.
    int renderChit (float* dst, int want, int maxLen, Rng& rng, float target, float rate, float duty, float scatter)
    {
        const float site = 180.f + (float) rng.irand (14) * 55.f;
        const float semis = 12.f * std::log2 (site / 400.f);
        float freq = (target / rate) * std::pow (2.f, semis * scatter / 12.f);
        freq = std::clamp (freq, 20.f, 16000.f);

        const double period = sampleRate / (double) freq;
        const int room = std::max (2, maxLen);
        int cycles = std::max (1, (int) std::lround ((double) want / period));
        auto lenFor = [&] (int c)
        {
            return std::max (1, (int) std::lround ((double) c * period));
        };
        while (cycles > 1 && lenFor (cycles) > room)
            --cycles;
        int len = std::min (lenFor (cycles), room);
        if (len < 2)
            len = 2;
        for (int i = 0; i < len; ++i)
        {
            const double frac = (double) (((long long) i * cycles) % len) / (double) len;
            const float pulse = frac < (double) duty ? 1.f : -1.f;
            dst[i] = pulse * 0.55f;
        }
        return len;
    }

    int beatCellNow() const
    {
        if (mix.restartQuarters <= 0.02f)
            return 0;
        const double pos = mix.slowRetrig ? clockPpq - syncOriginPpq : clockPpq;
        return (int) std::floor (pos / (double) mix.restartQuarters);
    }

    // A new note restarts the clocks whose retrig switch is on.
    // LFOs go back to the top of their shape. Delay drops its tail.
    // Speed counts its division from this note, so speech and Repeat
    // begin again with the key instead of the host clock.
    void retrigFromNote (const ClickParams& params)
    {
        for (int i = 0; i < ClickParams::lfoCount; ++i)
        {
            if (params.lfoRetrig[i] == 0)
                continue;
            lfoPhase[i] = 0.f;
            lfoRng[i] = lfoRng[i] * 1664525u + 1013904223u;
            if (lfoRng[i] == 0)
                lfoRng[i] = 1;
            lfoHold[i] = ((lfoRng[i] >> 8) / 16777216.f) * 2.f - 1.f;
        }
        if (params.delayRetrig != 0 && ! delay.empty())
            std::fill (delay.begin(), delay.end(), 0.f);
        if (params.slowRetrig != 0)
        {
            syncOriginPpq = clockPpq;
            scatterOrigin = scatterClock;
        }
    }

    void latchLfoBases()
    {
        lfoBaseCut = mix.filterHz;
        lfoBaseQ = mix.filterQ;
        lfoBaseDelay = mix.delaySec;
        lfoBaseEcho = mix.echo;
        lfoBaseWet = mix.crushMix;
        lfoBaseGlitch = mix.glitch;
        lfoBaseSlow = mix.slow;
        lfoBaseLevel = mix.level;
        lfoBaseDb = 20.f * std::log10 (std::max (mix.level, 1.0e-5f));
        lfoBaseBits = mix.bits;
        lfoBaseHold = mix.hold;
        lfoBaseFb = mix.feedback;
        lfoBaseScatter = mix.scatter;
        lfoBaseDouble = mix.doublet;
        lfoBaseTune = mix.subTune;
        lfoBaseSub = mix.subMix;
        lfoBaseShift = mix.formShift;
        lfoBaseSpread = mix.formSpread;
        lfoBaseBreath = mix.formBreath;
        lfoBaseDamp = mix.resoDamp;
        lfoBaseRTune = mix.resoTune;
        lfoBaseStack = mix.resoStack;
        lfoBaseAttack = mix.attack;
        lfoBaseDecay = mix.decay;
        lfoBaseSustain = mix.sustain;
        lfoBaseRelease = mix.release;
        lfoBaseFAttack = mix.fAttack;
        lfoBaseFDecay = mix.fDecay;
        lfoBaseFSustain = mix.fSustain;
        lfoBaseFRelease = mix.fRelease;
        lfoBaseFiltAmt = mix.filtAmt;
        lfoBaseSeed = mix.seed;
        lfoBaseSteps = mix.steps;
        lfoBaseWidth = mix.width;
        lfoBaseSamDrag = mix.samDrag;
    }

    // Repeat: each source walks a fixed run of pitches. Warble lives on the
    // bus (tapeStage), so the sources sit still in that mode.
    void updateScatter (Voice& voice)
    {
        const float depth = std::clamp (mix.scatter, 0.f, 1.f);
        if (depth < 0.0008f || mix.scatMode == 0)
        {
            voice.scatTarget = 1.f;
            voice.scatMul = 1.f;
            return;
        }
        const int clockAt = mix.slowRetrig ? scatterClock - scatterOrigin : scatterClock;
        const double seconds = (double) clockAt / std::max (1.0, sampleRate);
        int step = 0;
        const int n = std::clamp ((int) std::lround (mix.steps), 1, 16);
        if (mix.warbleHz > 0.05f)
        {
            const double window = 1.0 / (double) std::clamp (mix.warbleHz, 0.05f, 40.f);
            double phase = std::fmod (seconds, window) / window;
            if (phase < 0.0)
                phase = 0.0;
            step = std::min (n - 1, (int) (phase * (double) n));
        }
        else
        {
            double phase = 0.0;
            if (mix.sync && mix.restartQuarters > 0.02f)
            {
                const double q = (double) mix.restartQuarters;
                const double at = mix.slowRetrig ? clockPpq - syncOriginPpq : clockPpq;
                double pos = std::fmod (at, q);
                if (pos < 0.0)
                    pos += q;
                phase = pos / q;
            }
            else
            {
                const double window = std::clamp ((double) mix.slow, 0.05, 4.0);
                phase = std::fmod (seconds, window) / window;
            }
            if (phase < 0.0)
                phase = 0.0;
            if (phase >= 1.0)
                phase = 0.0;
            step = std::min (n - 1, (int) (phase * (double) n));
        }
        if (step != voice.scatStep)
        {
            voice.scatStep = step;
            uint32_t s = voice.hash ^ (uint32_t) std::lround (std::clamp (mix.seed, 0.f, 999.f) * 17.f);
            s ^= (uint32_t) step * 0x9E3779B9u;
            s ^= 0x85EBCA6Bu;
            s = s * 1664525u + 1013904223u;
            const float u = ((s >> 8) / 16777216.f) * 2.f - 1.f;
            voice.scatTarget = std::pow (2.f, u * depth);
        }
        const float glide = std::clamp (mix.scatterGlide, 0.f, 4.f);
        if (glide < 0.001f)
            voice.scatMul = voice.scatTarget;
        else
        {
            const float coef = 1.f / std::max (1.f, glide * (float) sampleRate);
            voice.scatMul += coef * (voice.scatTarget - voice.scatMul);
        }
    }

    void advanceFiltEnv()
    {
        bool held = false;
        for (const auto& voice : voices)
            if (voice.active && voice.held)
                held = true;
        if (! held && filtStage < 3)
            filtStage = 3;
        const float sustain = std::clamp (mix.fSustain, 0.f, 1.f);
        if (filtStage == 0)
        {
            filtEnv += 1.f / std::max (1.f, mix.fAttack * (float) sampleRate);
            if (filtEnv >= 1.f)
            {
                filtEnv = 1.f;
                filtStage = 1;
            }
        }
        else if (filtStage == 1)
        {
            const float span = std::max (0.0001f, 1.f - sustain);
            filtEnv -= span / std::max (1.f, mix.fDecay * (float) sampleRate);
            if (filtEnv <= sustain)
            {
                filtEnv = sustain;
                filtStage = 2;
            }
        }
        else if (filtStage == 2)
            filtEnv = sustain;
        else
        {
            filtEnv -= std::max (sustain, filtEnv) / std::max (1.f, mix.fRelease * (float) sampleRate);
            if (filtEnv <= 0.0008f)
                filtEnv = 0.f;
        }
        filtEnv = std::clamp (filtEnv, 0.f, 1.f);
    }

    static float bendLin (float base, float lo, float hi, float m)
    {
        if (std::abs (m) < 1.0e-6f)
            return base;
        return std::clamp (base + m * 0.5f * (hi - lo), lo, hi);
    }

    static float bendLog (float base, float lo, float hi, float m)
    {
        if (std::abs (m) < 1.0e-6f)
            return base;
        const float a = std::log (std::max (lo, 1.f));
        const float b = std::log (std::max (hi, lo + 1.f));
        float n = (std::log (std::clamp (base, lo, hi)) - a) / (b - a);
        n = std::clamp (n + m * 0.5f, 0.f, 1.f);
        return std::exp (a + n * (b - a));
    }

    // One bipolar step, -1 to 1. Sample-hold picks a new value each cycle.
    float lfoSample (int i)
    {
        float inc;
        if (mix.lfoSync[i] != 0 && mix.lfoQuarters[i] > 0.02f)
        {
            const float divSec = mix.lfoQuarters[i] * 60.f / (float) std::max (20.0, clockBpm);
            inc = 1.f / std::max (1.f, divSec * (float) sampleRate);
        }
        else
            inc = std::max (0.f, mix.lfoHz[i]) / (float) sampleRate;

        lfoPhase[i] += inc;
        bool wrapped = false;
        while (lfoPhase[i] >= 1.f)
        {
            lfoPhase[i] -= 1.f;
            wrapped = true;
        }
        if (wrapped && mix.lfoShape[i] == 5)
        {
            lfoRng[i] = lfoRng[i] * 1664525u + 1013904223u;
            lfoHold[i] = ((lfoRng[i] >> 8) / 16777216.f) * 2.f - 1.f;
        }

        const float p = lfoPhase[i];
        switch (mix.lfoShape[i])
        {
            case 1: return p < 0.5f ? p * 4.f - 1.f : 3.f - p * 4.f;
            case 2: return p * 2.f - 1.f;
            case 3: return 1.f - p * 2.f;
            case 4: return p < 0.5f ? 1.f : -1.f;
            case 5: return lfoHold[i];
            default: return std::sin (p * 6.2831853f);
        }
    }

    void applyLfo()
    {
        float wave[ClickParams::lfoCount];
        for (int i = 0; i < ClickParams::lfoCount; ++i)
        {
            wave[i] = lfoSample (i) * std::clamp (mix.lfoDepth[i], 0.f, 1.f);
            lfoOut[i] = wave[i];
        }
        auto amount = [&] (int dest)
        {
            float m = 0.f;
            for (int i = 0; i < ClickParams::lfoCount; ++i)
                if (mix.lfoDest[i] == dest)
                    m += wave[i];
            return m;
        };

        mix.filterHz = bendLog (lfoBaseCut, 40.f, 16000.f, amount (1));
        mix.filterQ = bendLin (lfoBaseQ, 0.3f, 8.f, amount (2));
        mix.delaySec = bendLin (lfoBaseDelay, 0.f, 16.f, amount (3));
        mix.echo = bendLin (lfoBaseEcho, 0.f, 1.f, amount (4));
        mix.crushMix = bendLin (lfoBaseWet, 0.f, 1.f, amount (5));
        mix.glitch = bendLin (lfoBaseGlitch, 0.f, 1.f, amount (6));
        if (mix.sync == 0)
            mix.slow = bendLin (lfoBaseSlow, 0.25f, 1.25f, amount (7));
        else
            mix.slow = lfoBaseSlow;
        const float levelMove = amount (8);
        if (std::abs (levelMove) < 1.0e-6f)
            mix.level = lfoBaseLevel;
        else
            mix.level = std::pow (10.f, bendLin (lfoBaseDb, -18.f, 12.f, levelMove) / 20.f);
        mix.bits = bendLin (lfoBaseBits, 2.f, 16.f, amount (9));
        mix.hold = bendLin (lfoBaseHold, 1.f, 80.f, amount (10));
        mix.feedback = bendLin (lfoBaseFb, 0.f, 0.99f, amount (11));
        mix.scatter = bendLin (lfoBaseScatter, 0.f, 1.f, amount (12));
        mix.subTune = bendLin (lfoBaseTune, -36.f, 36.f, amount (13));
        mix.subMix = bendLin (lfoBaseSub, 0.f, 1.f, amount (14));
        mix.formShift = bendLin (lfoBaseShift, 0.5f, 2.f, amount (15));
        mix.formSpread = bendLin (lfoBaseSpread, 0.25f, 2.5f, amount (16));
        mix.formBreath = bendLin (lfoBaseBreath, 0.f, 1.f, amount (17));
        mix.resoDamp = bendLin (lfoBaseDamp, 0.f, 1.f, amount (18));
        mix.resoTune = bendLin (lfoBaseRTune, -24.f, 24.f, amount (19));
        mix.resoStack = bendLin (lfoBaseStack, 0.f, 1.f, amount (20));
        mix.attack = bendLog (std::max (0.001f, lfoBaseAttack), 0.001f, 4.f, amount (21));
        mix.decay = bendLog (std::max (0.001f, lfoBaseDecay), 0.001f, 4.f, amount (22));
        mix.sustain = bendLin (lfoBaseSustain, 0.f, 1.f, amount (23));
        mix.release = bendLog (std::max (0.001f, lfoBaseRelease), 0.001f, 4.f, amount (24));
        mix.filtAmt = bendLin (lfoBaseFiltAmt, -1.f, 1.f, amount (25));
        samSpeedMul = bendLin (1.f, 0.25f, 4.f, amount (26));
        samPitchMul = bendLin (1.f, 0.5f, 2.f, amount (27));
        mix.seed = bendLin (lfoBaseSeed, 0.f, 42.f, amount (28));
        mix.fAttack = bendLog (std::max (0.001f, lfoBaseFAttack), 0.001f, 4.f, amount (29));
        mix.fDecay = bendLog (std::max (0.001f, lfoBaseFDecay), 0.001f, 4.f, amount (30));
        mix.fSustain = bendLin (lfoBaseFSustain, 0.f, 1.f, amount (31));
        mix.fRelease = bendLog (std::max (0.001f, lfoBaseFRelease), 0.001f, 4.f, amount (32));
        mix.steps = bendLin (lfoBaseSteps, 1.f, 16.f, amount (33));
        mix.width = bendLin (lfoBaseWidth, 0.05f, 0.95f, amount (34));
        mix.samDrag = bendLin (lfoBaseSamDrag, 0.f, 1.f, amount (35));
    }

    double sampleRate = 44100.0;
    double clockPpq = 0;
    double clockBpm = 120;
    int cap = 0;
    Voice voices[8];
    uint32_t bornCounter = 0;
    ClickParams mix;
    std::vector<float> delay;
    int delayLen = 0;

    // Tape stage. See tapeStage().
    std::vector<float> tapeRing;
    int tapeLen = 0;
    int tapeWrite = 0;
    bool tapeOn = true;
    float tapeBase = 0.f;      // smoothed head position, samples
    float tapeDepth = 0.f;     // smoothed sway, samples
    float tapeCoef = 0.001f;
    float tapeRate = 1.2f;
    float tapeFlutAmp = 0.18f;
    float tapeWander = 0.f;
    float tapeWanderSec = 0.1f;
    float tapeWowInc = 0.f;
    float tapeFlutInc = 0.f;
    float tapeDriftInc = 0.f;
    float tapeWowPhase = 0.f;
    float tapeFlutPhase = 0.f;
    float tapeDriftPhase = 0.f;
    int tapeDriftStep = -1;
    float tapeDriftFrom = 0.f;
    float tapeDriftTo = 0.f;
    uint32_t tapeSeedHash = 0;
    float tapeSeedPhase = 0.f;
    int write = 0;
    float smoothedDelay = 0.f;
    bool delayReady = false;
    float filtIc1 = 0.f;
    float filtIc2 = 0.f;
    float formIc1[3] {};
    float formIc2[3] {};
    uint32_t formRng = 1u;
    float resoDull = 0.f;
    std::vector<float> reso;
    int resoLen = 0;
    int resoWrite = 0;
    float smoothedCut = 4000.f;
    float smoothedQ = 0.7f;
    bool filterReady = false;
    float holdLevel (float x)
    {
        if (mix.compRatio <= 1.001f && std::abs (mix.compGain) < 0.05f)
            return x;
        const float ax = std::abs (x);
        const float follow = ax > compEnv ? 0.2f : 0.002f;
        compEnv += follow * (ax - compEnv);
        const float thresh = std::pow (10.f, std::clamp (mix.compThresh, -40.f, 0.f) / 20.f);
        float gain = 1.f;
        if (compEnv > thresh && thresh > 1.0e-6f)
        {
            const float over = compEnv / thresh;
            const float ratio = std::max (1.f, mix.compRatio);
            gain = std::pow (over, (1.f / ratio) - 1.f);
        }
        gain *= std::pow (10.f, std::clamp (mix.compGain, 0.f, 24.f) / 20.f);
        return std::clamp (x * gain, -1.f, 1.f);
    }

    void tuneReverb()
    {
        const float scale = (float) sampleRate / 44100.f;
        const int combBase[4] = { 1116, 1188, 1277, 1356 };
        const int allBase[2] = { 225, 556 };
        for (int i = 0; i < 4; ++i)
            revCombLen[i] = std::clamp ((int) std::lround ((float) combBase[i] * scale), 8, revCap - 1);
        for (int i = 0; i < 2; ++i)
            revAllLen[i] = std::clamp ((int) std::lround ((float) allBase[i] * scale), 8, revCap - 1);
        clearReverb();
    }

    void clearReverb()
    {
        for (int i = 0; i < 4; ++i)
        {
            std::fill (revComb[i], revComb[i] + revCap, 0.f);
            revCombPos[i] = 0;
            revStore[i] = 0.f;
        }
        for (int i = 0; i < 2; ++i)
        {
            std::fill (revAll[i], revAll[i] + revCap, 0.f);
            revAllPos[i] = 0;
        }
    }

    // A small room. Mix 0 returns the sample unchanged.
    float applyReverb (float x)
    {
        if (revMixLive <= 0.0008f)
            return x;
        const float fb = 0.25f + revSizeLive * 0.68f;
        const float d = revDampLive * 0.65f;
        const float in = x * 0.15f;
        float wet = 0.f;
        for (int i = 0; i < 4; ++i)
        {
            const int len = revCombLen[i];
            if (len < 2)
                continue;
            int p = revCombPos[i];
            if (p >= len)
                p = 0;
            const float y = revComb[i][p];
            revStore[i] = y * (1.f - d) + revStore[i] * d;
            revComb[i][p] = in + revStore[i] * fb;
            revCombPos[i] = p + 1 < len ? p + 1 : 0;
            wet += y;
        }
        wet *= 0.25f;
        for (int i = 0; i < 2; ++i)
        {
            const int len = revAllLen[i];
            if (len < 2)
                continue;
            int p = revAllPos[i];
            if (p >= len)
                p = 0;
            const float buf = revAll[i][p];
            const float out = -wet + buf;
            revAll[i][p] = wet + buf * 0.5f;
            revAllPos[i] = p + 1 < len ? p + 1 : 0;
            wet = out;
        }
        const float dryG = std::cos (revMixLive * 1.5707963f);
        const float wetG = std::sin (revMixLive * 1.5707963f);
        return x * dryG + wet * wetG;
    }

    // Ceiling after the output level. Dist bends the note into that ceiling.
    // Dist 0 and a ceiling above 0 dB leave the sample alone.
    float applyLimit (float x) const
    {
        float y = x;
        if (limitDistLive > 0.001f)
        {
            const float g = 1.f + limitDistLive * 8.f;
            y = std::tanh (y * g) / std::tanh (g);
        }
        if (limitLive > 0.f)
            return y;
        const float ceil = std::pow (10.f, std::clamp (limitLive, -24.f, 0.f) / 20.f);
        const float ay = std::abs (y);
        if (ay <= ceil)
            return y;
        return std::copysign (ceil, y);
    }

    float lfoPhase[4] {};
    float lfoOut[4] {};
    std::atomic<float> lfoVis[4] {};
    float lfoHold[4] {};
    uint32_t lfoRng[4] { 1u, 2u, 3u, 4u };
    float compEnv = 0.f;
    CrushMem busCrush;
    bool smoothReady = false;
    float slewCoef = 0.2f;
    float polyCoef = 0.05f;
    float polyGain = 1.f;
    float slewLevel = 1.f;
    float slewSrc = 1.f;
    float slewSubLvl = 1.f;
    float slewEcho = 0.f;
    float slewDelayLvl = 1.f;
    float slewFb = 0.f;
    float slewCrush = 1.f;
    float slewFilt = 1.f;
    float slewComp = 1.f;
    float slewSub = 0.f;
    float slewWidth = 0.5f;
    float slewSubWidth = 0.5f;
    float slewGlitch = 0.f;
    float slewLimit = 6.f;
    float limitLive = 6.f;
    float slewDist = 0.f;
    float limitDistLive = 0.f;
    float slewRev = 0.f;
    float slewRevSize = 0.45f;
    float slewRevDamp = 0.4f;
    float revMixLive = 0.f;
    float revSizeLive = 0.45f;
    float revDampLive = 0.4f;
    static constexpr int revCap = 8192;
    float revComb[4][revCap] {};
    float revAll[2][revCap] {};
    int revCombLen[4] {};
    int revAllLen[2] {};
    int revCombPos[4] {};
    int revAllPos[2] {};
    float revStore[4] {};
    int clickStampNow = -1;
    float busLast = 0.f;
    float busFadeFrom = 0.f;
    int busFadeLeft = 0;
    int busFadeSpan = 1;
    float lfoBaseCut = 4000.f;
    float lfoBaseQ = 0.7f;
    float lfoBaseDelay = 0.f;
    float lfoBaseEcho = 0.f;
    float lfoBaseWet = 1.f;
    float lfoBaseGlitch = 0.f;
    float lfoBaseSlow = 1.f;
    float lfoBaseLevel = 1.f;
    float lfoBaseDb = 0.f;
    float lfoBaseBits = 8.f;
    float lfoBaseHold = 1.f;
    float lfoBaseFb = 0.f;
    float lfoBaseScatter = 0.f;
    float lfoBaseDouble = 0.f;
    float lfoBaseTune = 0.f;
    float lfoBaseSub = 0.f;
    float lfoBaseShift = 1.f;
    float lfoBaseSpread = 1.f;
    float lfoBaseBreath = 0.f;
    float lfoBaseDamp = 0.f;
    float lfoBaseRTune = 0.f;
    float lfoBaseStack = 0.f;
    float lfoBaseAttack = 0.01f;
    float lfoBaseDecay = 0.12f;
    float lfoBaseSustain = 0.85f;
    float lfoBaseRelease = 0.28f;
    float lfoBaseFAttack = 0.01f;
    float lfoBaseFDecay = 0.18f;
    float lfoBaseFSustain = 0.f;
    float lfoBaseFRelease = 0.2f;
    float lfoBaseFiltAmt = 0.f;
    float lfoBaseSeed = 1.f;
    float lfoBaseSteps = 5.f;
    float lfoBaseWidth = 0.5f;
    float lfoBaseSamDrag = 1.f;
    int filtStage = 2;
    float filtEnv = 0.f;
    int scatterClock = 0;
    int scatterOrigin = 0;     // Repeat counts from here while speed retrig is on
    double syncOriginPpq = 0;  // the beat division counts from here while speed retrig is on
    float samPitchMul = 1.f;
    float samSpeedMul = 1.f;
    float bendSemis = 0.f;
    float bendMul = 1.f;
    int shapeStamp = -1;

    struct SpeechLoop
    {
        // Room for a couple of seconds, so a typical word never reallocates.
        SpeechLoop() { samples.reserve (96000); }
        std::vector<float> samples;
        int len = 0;
        float refHz = 106.f;
        double renderedRate = 44100.0;
    };
    SpeechLoop speech[3];
    std::atomic<int> speechSlot { 0 };
    int monoNote[16] {};
    float monoVel[16] {};
    int monoN = 0;

    int copyRing (const float* ring, float* dst, int maxN) const
    {
        if (dst == nullptr || maxN <= 0)
            return 0;
        const uint32_t published = scopePub.load (std::memory_order_acquire);
        const int have = (int) std::min<uint32_t> (published, (uint32_t) scopeN);
        const int take = std::min (maxN, have);
        for (int i = 0; i < take; ++i)
        {
            const uint32_t idx = published - (uint32_t) take + (uint32_t) i;
            dst[i] = ring[idx % (uint32_t) scopeN];
        }
        return take;
    }

    static constexpr int scopeN = 16384;
    float scopeRing[scopeN] {};
    float samRing[scopeN] {};
    uint32_t scopeWrite = 0;
    std::atomic<uint32_t> scopePub { 0 };
    std::atomic<float> scopeHz { 0.f };
};

// Two peaks at A2. Each octave above that shows twice as many,
// up to twelve, so a higher note draws more peaks. The filter only
// picks the trigger. hintHz is the lowest note; without one, the
// search stays in the bass (30–150 Hz).
struct ScopeLock
{
    int start = 0;
    int length = 0;
    double period = 0;
};

inline ScopeLock lockBassScope (const float* raw, int n, double sr, double hintHz, double previousPeriod)
{
    ScopeLock out;
    if (raw == nullptr || n < 32 || sr < 1000.0)
        return out;

    const bool hinted = hintHz >= 20.0 && hintHz < 5000.0;
    const double fmin = hinted ? hintHz * 0.8 : 30.0;
    const double fmax = hinted ? hintHz * 1.25 : 150.0;
    const double fc = hinted ? std::min (hintHz * 1.6, sr * 0.45) : 180.0;
    const float alpha = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * fc / sr));

    std::vector<float> filt ((size_t) n);
    float pole[4] = {};
    for (int i = 0; i < n; ++i)
    {
        float y = raw[i];
        for (float& z : pole)
        {
            z += alpha * (y - z);
            y = z;
        }
        filt[(size_t) i] = y;
    }

    const int tail = std::min (n, 4096);
    std::vector<float> mag ((size_t) tail);
    for (int i = 0; i < tail; ++i)
        mag[(size_t) i] = std::abs (filt[(size_t) (n - tail + i)]);
    const int rank = std::min (tail - 1, (int) (tail * 0.9));
    std::nth_element (mag.begin(), mag.begin() + rank, mag.end());
    const float gate = std::max (0.0015f, 0.12f * mag[(size_t) rank]);

    std::vector<int> hits;
    float trough = 0.f;
    for (int i = 1; i < n; ++i)
    {
        trough = std::min (trough, filt[(size_t) (i - 1)]);
        if (filt[(size_t) (i - 1)] <= 0.f && filt[(size_t) i] > 0.f)
        {
            if (trough <= -gate)
                hits.push_back (i);
            trough = 0.f;
        }
    }

    const double minPeriod = sr / fmax;
    const double maxPeriod = sr / fmin;
    double measured = 0;
    if ((int) hits.size() >= 3)
    {
        for (int end = (int) hits.size() - 1; end >= 2; --end)
        {
            const double a = (double) (hits[(size_t) end] - hits[(size_t) (end - 1)]);
            const double b = (double) (hits[(size_t) (end - 1)] - hits[(size_t) (end - 2)]);
            if (a < minPeriod || a > maxPeriod || b < minPeriod || b > maxPeriod)
                continue;
            const double center = 0.5 * (a + b);
            if (std::abs (a - b) > 0.14 * center)
                continue;
            measured = center;
            break;
        }
    }

    double period = measured;
    if (period <= 0.0 && hinted)
        period = sr / hintHz;
    if (period <= 0.0)
        period = previousPeriod;
    if (previousPeriod > 16.0 && period > 16.0 && std::abs (period - previousPeriod) < 0.08 * previousPeriod)
        period = 0.85 * previousPeriod + 0.15 * period;

    int window = std::min (n, (int) (sr * 0.02));
    if (period >= 8.0)
    {
        double cycles = 2.0;
        if (hinted)
            cycles = std::clamp (std::round (hintHz / 55.0), 2.0, 12.0);
        window = (int) std::lround (cycles * period);
    }
    window = std::clamp (window, 32, n);
    int start = n - window;
    if (period >= 8.0)
        for (int k = (int) hits.size() - 1; k >= 0; --k)
            if (hits[(size_t) k] + window <= n)
            {
                start = hits[(size_t) k];
                break;
            }

    out.start = start;
    out.length = std::min (window, n - start);
    out.period = period;
    return out;
}
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

/* The public-page clicks, played from a keyboard.
   A blip is a stepped pulse. A chit is a short pulse plus noise.
   Crush, hold, and glitch run on the live output. Scatter's
   gesture comes from the note and a seed, so the same key
   repeats. MIDI note sets the pitch. */

struct ClickParams
{
    int voice = 0;
    float bits = 4.f;
    float hold = 14.f;
    float glitch = 1.f;
    float slow = 0.46f;
    float scatter = 1.f;
    float seed = 1.f;
    float doublet = 0.4f;
    float delaySec = 1.f;
    float feedback = 0.3f;
    float echo = 0.5f;
    float level = 1.f;
    int poly = 1;
    float attack = 0.01f;
    float decay = 0.12f;
    float sustain = 0.85f;
    float release = 0.28f;
};

class ClicksEngine
{
    struct Voice;

public:
    void prepare (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 44100.0;
        cap = (int) std::ceil (sampleRate * 0.55) + 4;
        for (auto& voice : voices)
        {
            voice.active = false;
            voice.nLayers = 0;
            for (auto& layer : voice.layer)
            {
                layer.buf.assign ((size_t) cap, 0.f);
                layer.playing = false;
            }
        }
        delayLen = (int) std::ceil (sampleRate) + 4;
        delay.assign ((size_t) delayLen, 0.f);
        write = 0;
        delayReady = false;
        bendSemis = 0.f;
        bendMul = 1.f;
        monoN = 0;
    }

    void reset()
    {
        for (auto& voice : voices)
            voice.active = false;
        std::fill (delay.begin(), delay.end(), 0.f);
        write = 0;
        delayReady = false;
        monoN = 0;
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

    void noteOn (int note, float velocity, const ClickParams& params)
    {
        if (velocity <= 0.f || cap < 16)
            return;

        if (params.poly <= 0)
        {
            for (int i = 1; i < 8; ++i)
            {
                if (voices[i].active && voices[i].held)
                {
                    voices[i].held = false;
                    voices[i].stage = 3;
                }
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
                {
                    voices[0].held = false;
                    voices[0].stage = 3;
                }
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
        {
            oldest->held = false;
            oldest->stage = 3;
        }
    }

    void releaseAll()
    {
        monoN = 0;
        for (auto& voice : voices)
        {
            if (voice.active && voice.held)
            {
                voice.held = false;
                voice.stage = 3;
            }
        }
    }

    void trigger (Voice* voice, int note, float velocity, const ClickParams& params)
    {
        voice->active = true;
        voice->held = true;
        voice->note = note;
        voice->stage = 0;
        voice->env = 0.f;
        voice->gain = std::pow (std::clamp (velocity, 0.f, 1.f), 0.8f) * 0.55f;
        voice->baseSlow = std::clamp (params.slow, 0.05f, 4.f);
        voice->born = ++bornCounter;
        voice->nLayers = 0;
        voice->holdLeft = 0;
        voice->crushed = 0.f;
        voice->muteLeft = 0;
        voice->stutter = 0.f;

        const uint32_t userSeed = (uint32_t) std::lround (std::clamp (params.seed, 0.f, 999.f));
        const uint32_t seed = ((uint32_t) note * 1315423911u)
            ^ (userSeed * 747796405u + 0x9E3779B9u);
        voice->hash = seed == 0 ? 1u : seed;
        Rng rng (voice->hash);

        int mode = params.voice;
        if (mode <= 0)
        {
            const float roll = rng.next();
            if (roll < 0.45f)      mode = 1;
            else if (roll < 0.85f) mode = 2;
            else                   mode = 3;
        }

        const float target = midiHz (note);
        if (mode == 1 || mode == 3)
            addLayer (*voice, rng, 0, params, target, false);
        if (mode == 2 || mode == 3)
            addLayer (*voice, rng, 1, params, target, mode == 3);
    }

    void process (float* const* outputs, int numChannels, int numSamples)
    {
        if (outputs == nullptr || numChannels <= 0 || numSamples <= 0)
            return;

        const float echo = std::clamp (mix.echo, 0.f, 1.f);
        const float dryGain = std::cos (echo * 1.5707963f);
        const float wetGain = std::sin (echo * 1.5707963f);
        const float feedback = std::clamp (mix.feedback, 0.f, 0.85f);
        const float level = mix.level;
        const float targetDelay = std::clamp (mix.delaySec, 0.f, 1.f) * (float) sampleRate;

        if (! delayReady)
        {
            smoothedDelay = targetDelay;
            delayReady = true;
        }

        followTone();

        for (int i = 0; i < numSamples; ++i)
        {
            float x = 0.f;
            for (auto& voice : voices)
            {
                if (! voice.active)
                    continue;
                const float slowRatio = std::clamp (mix.slow, 0.05f, 4.f) / std::max (0.05f, voice.baseSlow);
                float s = 0.f;
                for (int n = 0; n < voice.nLayers; ++n)
                {
                    if (! voice.layer[n].playing)
                        continue;
                    s += sampleLayer (voice.layer[n], voice.layer[n].rate * slowRatio);
                }
                s = degrade (voice, s);
                const float env = advanceEnv (voice);
                if (! voice.active)
                    continue;
                x += s * voice.gain * env;
            }

            smoothedDelay += 0.002f * (targetDelay - smoothedDelay);
            const float y = readDelay (smoothedDelay);
            float into = x * wetGain + y * feedback;
            into = std::clamp (into, -2.f, 2.f);
            delay[(size_t) write] = into;
            write += 1;
            if (write >= delayLen)
                write = 0;

            const float out = (x * dryGain + y) * level;
            for (int c = 0; c < numChannels; ++c)
                outputs[c][i] = out;
        }
    }

    int activeVoices() const
    {
        int n = 0;
        for (const auto& voice : voices)
            if (voice.active)
                ++n;
        return n;
    }

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
        int delay = 0;
        bool skip = false;
        int skipDelay = 0;
        double skipRead = 0;
        float skipRate = 1.f;
        float skipGain = 0.2f;
        bool playing = false;
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
        float baseSlow = 0.46f;
        int holdLeft = 0;
        float crushed = 0.f;
        int muteLeft = 0;
        float stutter = 0.f;
        uint32_t hash = 1;
        uint32_t born = 0;
    };

    static float midiHz (int note)
    {
        return 440.f * std::pow (2.f, ((float) note - 69.f) / 12.f);
    }

    static float lerp (float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    static float readAt (const float* buf, int len, double pos)
    {
        if (len < 2 || pos >= (double) (len - 1))
            return 0.f;
        const int i = (int) pos;
        const float f = (float) (pos - i);
        return buf[i] * (1.f - f) + buf[i + 1] * f;
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

    float sampleLayer (Layer& layer, float play)
    {
        if (! layer.playing || layer.len < 2)
            return 0.f;
        const double span = (double) layer.len - 1.0;
        auto pull = [&] (double& read, float rate) -> float
        {
            if (read < 0.0)
                read = 0.0;
            while (read >= span)
                read -= span;
            const float s = readAt (layer.buf.data(), layer.len, read);
            read += (double) rate * (double) bendMul;
            while (read >= span)
                read -= span;
            return s;
        };

        float s = 0.f;
        if (layer.delay > 0)
            --layer.delay;
        else
            s += pull (layer.read, play);

        const float dub = std::clamp (mix.doublet, 0.f, 1.f);
        if (dub > 0.001f)
            s += pull (layer.skipRead, play * 1.8f) * (0.45f * dub);
        return s;
    }

    float degrade (Voice& voice, float sample)
    {
        const int bits = std::clamp ((int) std::lround (mix.bits), 2, 8);
        int hold = std::clamp ((int) std::lround (mix.hold), 1, 80);
        const float glitch = std::clamp (mix.glitch, 0.f, 1.f);
        hold += (int) std::lround (glitch * 24.f);
        if (voice.holdLeft <= 0)
        {
            voice.holdLeft = std::max (1, hold);
            const float steps = (float) ((1 << bits) - 1);
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
                {
                    voice.held = false;
                    voice.stage = 3;
                }
            }
        }

        const int stamp = mix.voice * 1000000
            + (int) std::lround (std::clamp (mix.scatter, 0.f, 1.f) * 200.f)
            + (int) std::lround (std::clamp (mix.seed, 0.f, 999.f)) * 1000;
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
            const bool held = voice.held;
            const float gain = voice.gain;
            const uint32_t born = voice.born;
            const int note = voice.note;
            trigger (&voice, note, 1.f, mix);
            voice.env = env;
            voice.stage = stage;
            voice.held = held;
            voice.gain = gain;
            voice.born = born;
        }
    }

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
            voice.env = sustain;
        }
        else
        {
            const float step = 1.f / std::max (1.f, mix.release * (float) sampleRate);
            voice.env -= step;
            if (voice.env <= 0.0008f)
            {
                voice.env = 0.f;
                voice.active = false;
            }
        }
        return std::max (0.f, voice.env);
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

    void addLayer (Voice& voice, Rng& rng, int index,
                    const ClickParams& params, float target, bool late)
    {
        if (voice.nLayers >= 2)
            return;
        Layer& layer = voice.layer[voice.nLayers++];
        const bool chit = index == 1;
        const float scatter = std::clamp (params.scatter, 0.f, 1.f);

        const float rate = std::clamp (
            lerp (params.slow, 0.32f + rng.next() * 0.28f, scatter),
            0.05f, 4.f);
        const float dur = chit
            ? lerp (0.23f, 0.12f + rng.next() * 0.22f, scatter)
            : lerp (0.36f, 0.22f + rng.next() * 0.28f, scatter);
        const float duty = chit
            ? 0.12f
            : std::clamp (lerp (0.28f, 0.15f + rng.next() * 0.35f, scatter), 0.04f, 0.9f);

        int length = (int) std::floor (sampleRate * (double) std::clamp (dur, 0.05f, 0.5f));
        length = std::clamp (length, 8, cap - 2);

        if (chit)
            renderChit (layer.buf.data(), length, rng, target, rate, duty, scatter);
        else
            renderBlip (layer.buf.data(), length, rng, target, rate, duty, scatter);

        layer.len = length;
        layer.read = 0;
        layer.rate = rate;
        layer.delay = late ? (int) std::lround (0.06 * sampleRate) : 0;
        layer.skipRead = 0;
        layer.playing = true;
    }

    void renderBlip (float* dst, int length, Rng& rng, float target, float rate, float duty, float scatter)
    {
        const int steps = 4 + rng.irand (5);
        float raw[8];
        const int n = std::min (steps, 8);
        double logSum = 0;
        for (int s = 0; s < n; ++s)
        {
            raw[s] = 70.f + (float) rng.irand (22) * 40.f;
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

        int seg = 0;
        float freq = raw[0];
        double phase = 0;
        for (int i = 0; i < length; ++i)
        {
            const int boundary = (int) std::floor (seg * (double) length / (double) n);
            if (i == boundary && seg < n)
            {
                freq = raw[seg];
                ++seg;
            }
            phase += (double) freq / sampleRate;
            phase -= std::floor (phase);
            const float pulse = phase < (double) duty ? 0.85f : -0.85f;
            dst[i] = pulse;
        }
    }

    void renderChit (float* dst, int length, Rng& rng, float target, float rate, float duty, float scatter)
    {
        const float site = 180.f + (float) rng.irand (14) * 55.f;
        const float semis = 12.f * std::log2 (site / 400.f);
        float freq = (target / rate) * std::pow (2.f, semis * scatter / 12.f);
        freq = std::clamp (freq, 20.f, 16000.f);
        double phase = 0;
        for (int i = 0; i < length; ++i)
        {
            phase += (double) freq / sampleRate;
            phase -= std::floor (phase);
            const float pulse = phase < (double) duty ? 1.f : -1.f;
            const float noise = rng.next() * 2.f - 1.f;
            dst[i] = pulse * 0.55f + noise * 0.7f;
        }
    }

    double sampleRate = 44100.0;
    int cap = 0;
    Voice voices[8];
    uint32_t bornCounter = 0;
    ClickParams mix;
    std::vector<float> delay;
    int delayLen = 0;
    int write = 0;
    float smoothedDelay = 0.f;
    bool delayReady = false;
    float bendSemis = 0.f;
    float bendMul = 1.f;
    int shapeStamp = -1;
    int monoNote[16] {};
    float monoVel[16] {};
    int monoN = 0;
};

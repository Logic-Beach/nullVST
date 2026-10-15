#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SamVoice.h"
#include "ESpeakVoice.h"

#include <cmath>
#include <cstdio>
#include <vector>

ClicksProcessor::ClicksProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    bindTaps();
    setSamText ("hello");
    apvts.state.setProperty ("waveid", 2, nullptr);
    apvts.state.setProperty ("sawdown", 1, nullptr);
    loadMidiMapFromState();
    requestSpeech();
}

ClicksProcessor::~ClicksProcessor()
{
    cancelPendingUpdate();
}

void ClicksProcessor::bindTaps()
{
    static const char* const ids[tapCount] = {
#define NG_TAP_NAME(n) #n,
        NG_TAPS (NG_TAP_NAME)
#undef NG_TAP_NAME
    };
    for (int i = 0; i < tapCount; ++i)
    {
        taps[i].raw = apvts.getRawParameterValue (ids[i]);
        taps[i].param = apvts.getParameter (ids[i]);
        // Every id above is in createLayout. A miss here is a typo.
        jassert (taps[i].raw != nullptr && taps[i].param != nullptr);
    }
}

void ClicksProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void ClicksProcessor::releaseResources()
{
    engine.reset();
}

bool ClicksProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

ClickParams ClicksProcessor::readParams() const
{
    ClickParams params;
    const int picked = juce::jlimit (0, 7, (int) std::lround (tap (tap_wave)));
    // Menu: sine, saw up, saw down, square, triangle, noise, Sam, eSpeak.
    // Engine 2 stays the rising saw. 8 is the falling saw.
    static const int engineWave[8] = { 1, 2, 8, 3, 4, 5, 6, 7 };
    params.wave = engineWave[picked];
    params.voice = (params.wave == 6 || params.wave == 7) ? 4 : 1;
    params.width = tap (tap_width);
    params.bits = tap (tap_bits);
    params.hold = tap (tap_hold);
    params.glitch = tap (tap_glitch);
    params.crushMix = tap (tap_crushmix);
    params.bitsOn = 1;
    params.crushOn = 1;
    int beats = 4;
    float bpm = 120.f;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto sig = pos->getTimeSignature())
                if (sig->numerator > 0)
                    beats = sig->numerator;
            if (auto b = pos->getBpm())
                if (*b > 1.0)
                    bpm = (float) *b;
        }
    auto tempoSeconds = [&] (int id, int syncId) -> float
    {
        const float freeSeconds = tap (id);
        if (tap (syncId) <= 0.5f)
            return freeSeconds;
        auto* p = taps[id].param;
        if (p == nullptr)
            return freeSeconds;
        const int index = ClickParams::divisionIndexNorm (p->getValue());
        return (float) (ClickParams::divisionQuarters (index, beats) * 60.0 / (double) bpm);
    };

    params.slow = tap (tap_slow);
    params.pitchFix = tap (tap_pitchfix) > 0.5f ? 1 : 0;
    params.sync = 0;
    params.restartQuarters = 0.f;
    params.slowRetrig = tap (tap_slowretrig) > 0.5f ? 1 : 0;
    if (tap (tap_slowsync) > 0.5f)
    {
        params.sync = 1;
        params.restartQuarters = (float) ClickParams::divisionQuarters (
            ClickParams::divisionIndex (params.slow), beats);
        params.slow = 1.f;
    }
    params.scatter = tap (tap_scatter);
    params.seed = tap (tap_seed);
    params.scatMode = (int) tap (tap_scatmode);
    params.steps = tap (tap_steps);
    params.scatterGlide = tap (tap_scatglide);
    params.warbleHz = tap (tap_wobrate);
    params.srcGlide = tap (tap_srcglide);
    params.doublet = 0.f;
    params.delaySec = tempoSeconds (tap_delay, tap_delaysync);
    params.delayRetrig = tap (tap_delayretrig) > 0.5f ? 1 : 0;
    params.feedback = tap (tap_feedback);
    params.echo = tap (tap_echo);
    params.poly = (int) tap (tap_poly);
    params.attack = tempoSeconds (tap_attack, tap_attacksync);
    params.decay = tempoSeconds (tap_decay, tap_decaysync);
    params.sustain = tap (tap_sustain);
    params.release = tempoSeconds (tap_release, tap_releasesync);
    params.subMode = (int) tap (tap_submode);
    params.subWave = (int) tap (tap_subwave);
    params.subTune = tap (tap_subtune);
    params.subFine = tap (tap_subfine);
    params.subMix = tap (tap_submix);
    params.subWidth = tap (tap_subwidth);
    params.subAttack = tempoSeconds (tap_subattack, tap_subattacksync);
    params.subDecay = tempoSeconds (tap_subdecay, tap_subdecaysync);
    params.subSustain = tap (tap_subsustain);
    params.subRelease = tempoSeconds (tap_subrelease, tap_subreleasesync);
    params.filterType = (int) tap (tap_filttype);
    params.filterPlace = (int) tap (tap_filtwhere);
    params.fxOrder = (int) tap (tap_fxorder);
    params.tapeSlot = (int) tap (tap_tapeslot);
    params.crushSlot = (int) tap (tap_crushslot);
    params.filtSlot = (int) tap (tap_filtslot);
    params.delaySlot = (int) tap (tap_delayslot);
    params.revSlot = (int) tap (tap_revslot);
    params.compSlot = (int) tap (tap_compslot);
    params.revSize = tap (tap_revsize);
    params.revDamp = tap (tap_revdamp);
    params.revMix = tap (tap_revmix);
    params.filterHz = tap (tap_filtcut);
    params.filterQ = tap (tap_filtq);
    params.formShift = tap (tap_formshift);
    params.formSpread = tap (tap_formspread);
    params.formBreath = tap (tap_formbreath);
    params.resoDamp = tap (tap_resodamp);
    params.resoTune = tap (tap_resotune);
    params.resoStack = tap (tap_resostack);
    params.fAttack = tempoSeconds (tap_fattack, tap_fattacksync);
    params.fDecay = tempoSeconds (tap_fdecay, tap_fdecaysync);
    params.fSustain = tap (tap_fsustain);
    params.fRelease = tempoSeconds (tap_frelease, tap_freleasesync);
    params.filtAmt = tap (tap_fenv);
    params.compThresh = tap (tap_compthresh);
    params.compRatio = tap (tap_compratio);
    params.compGain = tap (tap_compgain);
    params.limitDb = tap (tap_limit);
    params.limitDist = tap (tap_limitdist);
    params.samDrag = params.wave == 7 ? 1.f : tap (tap_samslow);
    const float db = tap (tap_level);
    params.level = std::pow (10.f, db / 20.f);
    params.srcLevel = tap (tap_srclevel);
    params.subLevel = tap (tap_sublevel);
    params.crushLevel = tap (tap_crushlevel);
    params.filtMix = tap (tap_filtmix);
    params.delayLevel = tap (tap_delaylevel);
    params.compMix = tap (tap_compmix);
    for (int i = 0; i < ClickParams::lfoCount; ++i)
    {
        const int base = tap_lfo1shape + i * 5;
        params.lfoShape[i] = (int) tap (base + 0);
        params.lfoHz[i] = tap (base + 1);
        params.lfoSync[i] = tap (base + 2) > 0.5f ? 1 : 0;
        params.lfoRetrig[i] = tap (tap_lfo1retrig + i) > 0.5f ? 1 : 0;
        params.lfoDepth[i] = tap (base + 3);
        params.lfoDest[i] = (int) tap (base + 4);
        if (params.lfoSync[i] != 0)
        {
            if (auto* p = taps[base + 1].param)
                params.lfoQuarters[i] = (float) ClickParams::divisionQuarters (
                    ClickParams::divisionIndexNorm (p->getValue()), beats);
        }
    }
    return params;
}

void ClicksProcessor::requestSpeech()
{
    speechRequest.store (true, std::memory_order_release);
}

bool ClicksProcessor::takeSpeechRequest()
{
    return speechRequest.exchange (false, std::memory_order_acq_rel);
}

void ClicksProcessor::replacePluginState (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (xml);
    if (! tree.hasProperty ("waveid"))
    {
        int wave = 0;
        int voice = -1;
        bool haveWave = false;
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto child = tree.getChild (i);
            const auto id = child.getProperty ("id").toString();
            const int value = (int) std::lround ((float) child.getProperty ("value", 0));
            if (id == "wave")
            {
                wave = value;
                haveWave = true;
            }
            else if (id == "voice")
                voice = value;
            else if (id == "lfo1dest" || id == "lfo2dest")
            {
                int next = value;
                if (value == 13)
                    next = 0;
                else if (value > 13)
                    next = value - 1;
                if (next != value)
                    child.setProperty ("value", next, nullptr);
            }
        }
        if (tree.hasProperty ("voice"))
            voice = (int) std::lround (tree.getProperty ("voice").toString().getFloatValue());
        if (! haveWave && tree.hasProperty ("wave"))
        {
            wave = (int) std::lround (tree.getProperty ("wave").toString().getFloatValue());
            haveWave = true;
        }
        const bool sam = voice >= 4 || (haveWave && wave >= 6);
        const int mapped = sam ? 5 : (wave <= 0 ? 0 : juce::jlimit (0, 5, wave - 1));
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto child = tree.getChild (i);
            if (child.getProperty ("id").toString() == "wave")
                child.setProperty ("value", mapped, nullptr);
        }
        tree.setProperty ("waveid", 2, nullptr);
    }
    if (! tree.hasProperty ("sawdown"))
    {
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto child = tree.getChild (i);
            const auto id = child.getProperty ("id").toString();
            const int value = (int) std::lround ((float) child.getProperty ("value", 0));
            int next = value;
            if (id == "wave" && value >= 2)
                next = value + 1;
            else if (id == "subwave" && value >= 3)
                next = value + 1;
            else if (id.startsWith ("lfo") && id.endsWith ("shape") && value >= 3)
                next = value + 1;
            if (next != value)
                child.setProperty ("value", next, nullptr);
        }
        tree.setProperty ("sawdown", 1, nullptr);
    }
    apvts.replaceState (tree);
    loadMidiMapFromState();
    currentProgram.store (juce::jlimit (0, getNumPrograms() - 1,
                                        (int) apvts.state.getProperty ("program", 0)));
    setSamText (apvts.state.getProperty ("sam", "hello").toString());
    requestSpeech();
}

juce::String ClicksProcessor::lockSpeech (bool announce)
{
    speechRequest.store (false, std::memory_order_release);
    const auto word = samText();
    const int wave = (int) std::lround (apvts.getRawParameterValue ("wave")->load());
    const bool espeak = wave == 7;
    const double hostRate = engine.engineSampleRate();
    const double useRate = hostRate > 1000.0 ? hostRate : 44100.0;
    const int maxLen = std::max (8, (int) std::ceil (useRate * 8.0));
    std::vector<float> buf ((size_t) maxLen, 0.f);
    int cut = 0;
    int n = 0;
    float refHz = 110.f;
    int speed = 72, pitch = 64, mouth = 128, throat = 128;
    if (espeak)
    {
        speed = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakrate")->load()), 80, 450);
        pitch = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakpitch")->load()), 0, 99);
        mouth = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakrange")->load()), 0, 99);
        throat = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakgap")->load()), 0, 20);
        const int variant = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakvoice")->load()), 0, 11);
        speakLiveRate = speed;
        speakLivePitch = pitch;
        speakLiveRange = mouth;
        speakLiveGap = throat;
        speakLiveVoice = variant;
        speechLiveKind = 1;
        samLiveWord = word;
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto bundled = exe.getParentDirectory().getSiblingFile ("Resources");
        juce::String dataPath;
        if (bundled.getChildFile ("espeak-ng-data").isDirectory())
            dataPath = bundled.getFullPathName();
        else if (exe.getParentDirectory().getChildFile ("espeak-ng-data").isDirectory())
            dataPath = exe.getParentDirectory().getFullPathName();
        if (dataPath.isNotEmpty())
            n = renderEspeakSpeech (buf.data(), maxLen, useRate, word.toRawUTF8(),
                                    speed, pitch, mouth, throat, variant, &cut, dataPath.toRawUTF8());
        refHz = 110.f + (float) (pitch - 50) * 1.5f;
    }
    else
    {
        speed = std::clamp ((int) std::lround (apvts.getRawParameterValue ("samspeed")->load()), 1, 255);
        pitch = std::clamp ((int) std::lround (apvts.getRawParameterValue ("sampitch")->load()), 1, 255);
        mouth = std::clamp ((int) std::lround (apvts.getRawParameterValue ("sammouth")->load()), 0, 255);
        throat = std::clamp ((int) std::lround (apvts.getRawParameterValue ("samthroat")->load()), 0, 255);
        samLiveSpeed = speed;
        samLivePitch = pitch;
        samLiveMouth = mouth;
        samLiveThroat = throat;
        speechLiveKind = 0;
        samLiveWord = word;
        n = renderSamSpeech (buf.data(), maxLen, useRate,
                             word.toRawUTF8(), speed, pitch, mouth, throat, &cut);
        refHz = 6805.6f / (float) pitch;
    }
    if (n >= 2)
        engine.setSpeech (buf.data(), n, refHz, useRate);

    const double secs = n >= 2 ? (double) n / useRate : 0.0;
    const char* who = espeak ? "eSpeak" : "Sam";
    juce::String status;
    if (espeak && n < 2 && cut == 0)
        status = "eSpeak could not speak that. The voice data may be missing from the plugin.";
    else if (n < 2 && cut)
        status = juce::String ("That line is too long for ") + who + ". It was left out.";
    else if (n < 2)
        status = juce::String (who) + " could not speak that.";
    else if (cut)
        status = "Locked \"" + word + "\" " + juce::String (secs, 1) + "s. The rest was left off.";
    else
        status = "Locked \"" + word + "\" " + juce::String (secs, 1) + "s.";
    if (announce)
        std::fprintf (stderr, "NULL GLITCH %s speed %d pitch %d mouth %d throat %d\n",
                      status.toRawUTF8(), speed, pitch, mouth, throat);
    return status;
}

bool ClicksProcessor::samNeedsRender() const
{
    const int wave = (int) std::lround (apvts.getRawParameterValue ("wave")->load());
    if (wave != 6 && wave != 7)
        return false;
    if (wave == 7)
    {
        const int rate = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakrate")->load()), 80, 450);
        const int pitch = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakpitch")->load()), 0, 99);
        const int range = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakrange")->load()), 0, 99);
        const int gap = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakgap")->load()), 0, 20);
        const int variant = std::clamp ((int) std::lround (apvts.getRawParameterValue ("espeakvoice")->load()), 0, 11);
        if (speechLiveKind != 1 || rate != speakLiveRate || pitch != speakLivePitch
            || range != speakLiveRange || gap != speakLiveGap || variant != speakLiveVoice)
            return true;
        return samText() != samLiveWord;
    }
    const int speed = std::clamp ((int) std::lround (apvts.getRawParameterValue ("samspeed")->load()), 1, 255);
    const int pitch = std::clamp ((int) std::lround (apvts.getRawParameterValue ("sampitch")->load()), 1, 255);
    const int mouth = std::clamp ((int) std::lround (apvts.getRawParameterValue ("sammouth")->load()), 0, 255);
    const int throat = std::clamp ((int) std::lround (apvts.getRawParameterValue ("samthroat")->load()), 0, 255);
    if (speechLiveKind != 0 || speed != samLiveSpeed || pitch != samLivePitch
        || mouth != samLiveMouth || throat != samLiveThroat)
        return true;
    return samText() != samLiveWord;
}

void ClicksProcessor::setSamText (const juce::String& raw)
{
    juce::String cleaned;
    for (auto c : raw)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || c == ' ' || c == '\'' || c == '.' || c == ',' || c == '?' || c == '!';
        if (ok)
            cleaned += c;
        if (cleaned.length() >= 40)
            break;
    }

    {
        const juce::SpinLock::ScopedLockType lock (samLock);
        if (cleaned == juce::String (samTextBuf))
            return;
        cleaned.copyToUTF8 (samTextBuf, (int) sizeof samTextBuf);
        samTextBuf[sizeof samTextBuf - 1] = 0;
    }
    apvts.state.setProperty ("sam", cleaned, nullptr);
    samRev.fetch_add (1, std::memory_order_release);
}

juce::String ClicksProcessor::samText() const
{
    const juce::SpinLock::ScopedLockType lock (samLock);
    return juce::String (samTextBuf);
}

void ClicksProcessor::renderRange (juce::AudioBuffer<float>& buffer, int start, int end)
{
    const int n = end - start;
    if (n <= 0)
        return;
    double bpm = 120.0;
    int beats = 4;
    bool host = false;
    double ppq = 0.0;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto got = pos->getBpm())
                bpm = std::clamp ((double) *got, 20.0, 400.0);
            if (auto sig = pos->getTimeSignature())
                if (sig->numerator > 0)
                    beats = sig->numerator;
            if (pos->getIsPlaying())
                if (auto at = pos->getPpqPosition())
                {
                    host = true;
                    const double step = (bpm / 60.0) / getSampleRate();
                    ppq = (double) *at + (double) start * step;
                }
        }
    engine.setClock (ppq, bpm, beats, host);
    float* chans[2] = { nullptr, nullptr };
    const int nc = std::min (buffer.getNumChannels(), 2);
    for (int c = 0; c < nc; ++c)
        chans[c] = buffer.getWritePointer (c) + start;
    engine.process (chans, nc, n);
}

void ClicksProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    // The on-screen keyboard is hidden, so the audio thread no longer feeds it.
    buffer.clear();

    const ClickParams params = readParams();
    engine.setMix (params);

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int at = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        renderRange (buffer, pos, at);
        if (msg.isNoteOn())
            engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity(), params);
        else if (msg.isNoteOff() || msg.isNoteOn (true))
            engine.noteOff (msg.getNoteNumber());
        else if (msg.isController())
        {
            if (handleMidiController (msg.getControllerNumber(), msg.getControllerValue()))
                continue;
        }
        else if (msg.isPitchWheel())
            engine.setPitchBend (static_cast<float> (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.f);
        else if (msg.isAllNotesOff())
            engine.releaseAll();
        else if (msg.isAllSoundOff())
            engine.reset();
        pos = at;
    }
    renderRange (buffer, pos, buffer.getNumSamples());
    meter.store (buffer.getMagnitude (0, buffer.getNumSamples()));
}

void ClicksProcessor::reset()
{
    engine.reset();
}

juce::AudioProcessorEditor* ClicksProcessor::createEditor()
{
    return new ClicksEditor (*this);
}

// Shipped presets. Each starts from init and changes a few knobs.
// A step with division true sets a synced time by its division index
// (0 = 1/32t … 7 = 1/8, 10 = 1/4, 13 = 1/2, 15 = 1 bar).
namespace
{
    struct Step
    {
        const char* id;
        float value;
        bool division = false;
    };
    struct Factory
    {
        const char* name;
        const char* say;
        std::vector<Step> steps;
    };

    const std::vector<Factory>& factoryPresets()
    {
        static const std::vector<Factory> table = {
            { "", "hello", {} },
            { "glass bell", "hello", {
                { "wave", 0 }, { "subwave", 4 }, { "subtune", 12.f }, { "sublevel", 0.35f },
                { "attack", 1, true }, { "decay", 13, true }, { "sustain", 0.2f }, { "release", 10, true },
                { "subattack", 1, true }, { "subdecay", 13, true }, { "subsustain", 0.2f }, { "subrelease", 10, true },
                { "delaysync", 1 }, { "delay", 9, true }, { "echo", 0.3f }, { "feedback", 0.4f },
                { "revmix", 0.35f }, { "revsize", 0.7f } } },
            { "tape bass", "hello", {
                { "wave", 1 }, { "poly", 0 }, { "width", 0.5f }, { "srcglide", 0.08f },
                { "subwave", 0 }, { "subtune", -12.f }, { "sublevel", 0.8f },
                { "attacksync", 0 }, { "decaysync", 0 }, { "releasesync", 0 },
                { "attack", 0.004f }, { "decay", 0.3f }, { "sustain", 0.7f }, { "release", 0.15f },
                { "subattacksync", 0 }, { "subattack", 0.004f }, { "subdecay", 0.3f }, { "subsustain", 0.7f }, { "subrelease", 0.15f },
                { "filttype", 1 }, { "filtcut", 900.f }, { "filtq", 1.2f }, { "fenv", 0.6f }, { "fdecay", 0.25f },
                { "limitdist", 0.2f } } },
            { "crushed keys", "hello", {
                { "wave", 3 }, { "width", 0.35f }, { "bits", 6.f }, { "hold", 4.f }, { "crushmix", 0.6f },
                { "attack", 1, true }, { "decay", 10, true }, { "sustain", 0.4f }, { "release", 7, true },
                { "subattack", 1, true }, { "subdecay", 10, true }, { "subsustain", 0.4f }, { "subrelease", 7, true },
                { "filttype", 1 }, { "filtcut", 3500.f }, { "filtq", 0.9f },
                { "delaysync", 1 }, { "delay", 9, true }, { "echo", 0.35f }, { "feedback", 0.45f } } },
            { "warble pad", "hello", {
                { "wave", 4 }, { "subwave", 4 }, { "sublevel", 0.5f },
                { "scatmode", 0 }, { "scatter", 0.25f }, { "wobrate", 0.6f }, { "scatglide", 0.4f },
                { "attack", 13, true }, { "decay", 13, true }, { "sustain", 0.8f }, { "release", 15, true },
                { "subattack", 13, true }, { "subrelease", 15, true },
                { "filttype", 1 }, { "filtcut", 1800.f }, { "filtq", 2.f },
                { "lfo1dest", 1 }, { "lfo1depth", 0.3f }, { "lfo1rate", 0.25f },
                { "revmix", 0.5f }, { "revsize", 0.8f }, { "revdamp", 0.3f } } },
            { "robot says", "null glitch", {
                { "wave", 6 }, { "sublevel", 0.3f }, { "subwave", 0 },
                { "sampitch", 64.f }, { "samspeed", 72.f },
                { "attack", 1, true }, { "release", 10, true }, { "subattack", 1, true }, { "subrelease", 10, true },
                { "bits", 8.f }, { "crushmix", 0.5f },
                { "delaysync", 1 }, { "delay", 10, true }, { "echo", 0.25f }, { "feedback", 0.3f } } },
            { "stutter repeat", "hello", {
                { "wave", 2 }, { "scatmode", 1 }, { "steps", 4.f }, { "scatter", 0.6f },
                { "slowsync", 1 }, { "slow", 0.6f },
                { "bits", 10.f }, { "crushmix", 0.4f },
                { "attack", 1, true }, { "decay", 10, true }, { "sustain", 0.6f }, { "release", 7, true },
                { "subattack", 1, true }, { "subrelease", 7, true },
                { "delaysync", 1 }, { "delay", 4, true }, { "echo", 0.3f }, { "feedback", 0.35f } } },
            { "noise hat", "hello", {
                { "wave", 5 }, { "submode", 0 }, { "sublevel", 0.f },
                { "attacksync", 0 }, { "decaysync", 0 }, { "releasesync", 0 },
                { "attack", 0.001f }, { "decay", 0.08f }, { "sustain", 0.f }, { "release", 0.05f },
                { "filttype", 2 }, { "filtcut", 6000.f }, { "filtq", 0.8f },
                { "level", 0.f } } },
        };
        return table;
    }
}

int ClicksProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

const juce::String ClicksProcessor::getProgramName (int index)
{
    const auto& table = factoryPresets();
    if (index < 0 || index >= (int) table.size())
        return {};
    return index == 0 ? juce::String ("init") : juce::String (table[(size_t) index].name);
}

// Hosts may call this from the audio thread, and some call it again with
// the saved index after a session restore. Only a change is applied, and
// always on the message thread.
void ClicksProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms() || index == currentProgram.load())
        return;
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        applyFactoryPreset (index);
        return;
    }
    wantedProgram.store (index);
    triggerAsyncUpdate();
}

void ClicksProcessor::handleAsyncUpdate()
{
    const int index = wantedProgram.exchange (-1);
    if (index >= 0)
        applyFactoryPreset (index);
}

void ClicksProcessor::applyFactoryPreset (int index)
{
    const auto& table = factoryPresets();
    if (index < 0 || index >= (int) table.size())
        return;
    const auto& preset = table[(size_t) index];
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
    for (const auto& step : preset.steps)
    {
        auto* param = apvts.getParameter (step.id);
        jassert (param != nullptr);
        if (param == nullptr)
            continue;
        const float norm = step.division
            ? juce::jlimit (0.f, 1.f, step.value / (float) (ClickParams::divisionCount() - 1))
            : param->convertTo0to1 (step.value);
        param->setValueNotifyingHost (norm);
    }
    setSamText (preset.say);
    apvts.state.setProperty ("preset", preset.name, nullptr);
    apvts.state.setProperty ("program", index, nullptr);
    currentProgram.store (index);
    requestSpeech();
    // With the panel closed nothing polls the request, so render the word here.
    if (getActiveEditor() == nullptr)
        lockSpeech (false);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void ClicksProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    saveMidiMapToState();
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ClicksProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            replacePluginState (*xml);
}

void ClicksProcessor::armMidiLearn (const juce::String& paramId)
{
    if (paramId.isEmpty() || apvts.getParameter (paramId) == nullptr)
        return;
    const juce::ScopedLock lock (midiMapLock);
    pendingLearnParam = paramId;
    learnArmed.store (true, std::memory_order_release);
}

void ClicksProcessor::cancelMidiLearn()
{
    const juce::ScopedLock lock (midiMapLock);
    pendingLearnParam.clear();
    learnArmed.store (false, std::memory_order_release);
}

// Call with midiMapLock held.
void ClicksProcessor::rebuildCcTargets()
{
    for (int cc = 0; cc < 128; ++cc)
    {
        const auto& id = midiCcToParam[(size_t) cc];
        ccTarget[(size_t) cc].store (id.isEmpty() ? nullptr : apvts.getParameter (id),
                                     std::memory_order_release);
    }
}

bool ClicksProcessor::isMidiLearning() const
{
    const juce::ScopedLock lock (midiMapLock);
    return pendingLearnParam.isNotEmpty();
}

juce::String ClicksProcessor::midiLearnTarget() const
{
    const juce::ScopedLock lock (midiMapLock);
    return pendingLearnParam;
}

juce::String ClicksProcessor::paramNameForId (const juce::String& paramId) const
{
    if (auto* p = apvts.getParameter (paramId))
        return p->getName (512);
    return paramId;
}

void ClicksProcessor::clearMidiMappingForParam (const juce::String& paramId)
{
    {
        const juce::ScopedLock lock (midiMapLock);
        for (auto& id : midiCcToParam)
            if (id == paramId)
                id.clear();
        rebuildCcTargets();
    }
    saveMidiMapToState();
}

int ClicksProcessor::midiCcForParam (const juce::String& paramId) const
{
    const juce::ScopedLock lock (midiMapLock);
    for (int i = 0; i < 128; ++i)
        if (midiCcToParam[(size_t) i] == paramId)
            return i;
    return -1;
}

void ClicksProcessor::applyMidiController (juce::RangedAudioParameter* param, int value)
{
    if (param == nullptr)
        return;
    const float norm = juce::jlimit (0.f, 1.f, (float) value / 127.f);
    param->beginChangeGesture();
    param->setValueNotifyingHost (norm);
    param->endChangeGesture();
}

// Audio thread. The plain path is one atomic load. Learning is the rare
// path and takes the lock once. The state tree is written later, on the
// message thread, by takeMidiLearnNotice or getStateInformation.
bool ClicksProcessor::handleMidiController (int cc, int value)
{
    if (cc < 0 || cc >= 128)
        return false;
    if (learnArmed.load (std::memory_order_acquire))
    {
        juce::RangedAudioParameter* learned = nullptr;
        {
            const juce::ScopedLock lock (midiMapLock);
            if (pendingLearnParam.isNotEmpty())
            {
                for (auto& id : midiCcToParam)
                    if (id == pendingLearnParam)
                        id.clear();
                midiCcToParam[(size_t) cc] = pendingLearnParam;
                learnNotifyParam = pendingLearnParam;
                pendingLearnParam.clear();
                learnArmed.store (false, std::memory_order_release);
                rebuildCcTargets();
                learned = ccTarget[(size_t) cc].load (std::memory_order_acquire);
            }
        }
        if (learned != nullptr)
        {
            mapDirty.store (true, std::memory_order_release);
            learnNotifyCc.store (cc);
            applyMidiController (learned, value);
            return true;
        }
    }
    auto* mapped = ccTarget[(size_t) cc].load (std::memory_order_acquire);
    if (mapped == nullptr)
        return false;
    applyMidiController (mapped, value);
    return true;
}

void ClicksProcessor::loadMidiMapFromState()
{
    const juce::ScopedLock lock (midiMapLock);
    midiCcToParam.fill ({});
    pendingLearnParam.clear();
    for (int i = 0; i < apvts.state.getNumProperties(); ++i)
    {
        const auto name = apvts.state.getPropertyName (i).toString();
        if (! name.startsWith ("midicc_"))
            continue;
        const int cc = name.substring (7).getIntValue();
        if (cc < 0 || cc > 127)
            continue;
        const auto id = apvts.state.getProperty (name).toString();
        if (id.isNotEmpty() && apvts.getParameter (id) != nullptr)
            midiCcToParam[(size_t) cc] = id;
    }
    learnArmed.store (false, std::memory_order_release);
    rebuildCcTargets();
}

int ClicksProcessor::takeMidiLearnNotice (juce::String& paramIdOut)
{
    if (mapDirty.exchange (false))
        saveMidiMapToState();
    const int cc = learnNotifyCc.exchange (-1);
    if (cc < 0)
        return -1;
    const juce::ScopedLock lock (midiMapLock);
    paramIdOut = learnNotifyParam;
    learnNotifyParam.clear();
    return cc;
}

// Message thread only. It writes the state tree.
void ClicksProcessor::saveMidiMapToState()
{
    mapDirty.store (false);
    const juce::ScopedLock lock (midiMapLock);
    for (int i = apvts.state.getNumProperties() - 1; i >= 0; --i)
    {
        const auto name = apvts.state.getPropertyName (i).toString();
        if (name.startsWith ("midicc_"))
            apvts.state.removeProperty (name, nullptr);
    }
    for (int cc = 0; cc < 128; ++cc)
    {
        const auto& id = midiCcToParam[(size_t) cc];
        if (id.isNotEmpty())
            apvts.state.setProperty ("midicc_" + juce::String (cc), id, nullptr);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ClicksProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto choice = AudioParameterChoiceAttributes().withAutomatable (true);
    const auto live = AudioParameterFloatAttributes().withAutomatable (true);
    const auto seconds = AudioParameterFloatAttributes().withAutomatable (true).withLabel ("s");
    const auto decibels = AudioParameterFloatAttributes().withAutomatable (true).withLabel ("dB");
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "wave", 1 },
        "Wave",
        StringArray { "Sine", "Saw up", "Saw down", "Square", "Triangle", "Noise", "Sam", "eSpeak" },
        0,
        choice));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "width", 1 }, "Width",
        NormalisableRange<float> (0.05f, 0.95f, 0.01f), 0.5f, live));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "poly", 1 },
        "Voices",
        StringArray { "Mono", "Poly" },
        1,
        choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "submode", 1 },
        "Sub",
        StringArray { "Off", "Add", "Subtract", "Multiply", "Divide", "AM", "FM", "Min", "Max" },
        1,
        choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "subwave", 1 },
        "Shape",
        StringArray { "Sine", "Square", "Saw up", "Saw down", "Triangle" },
        0,
        choice));
    const auto semis = AudioParameterFloatAttributes().withAutomatable (true).withLabel ("st");
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "subtune", 1 }, "Tune",
        NormalisableRange<float> (-36.f, 36.f, 0.01f), -12.f, semis));
    const auto cents = AudioParameterFloatAttributes().withAutomatable (true).withLabel ("ct");
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "subfine", 1 }, "Fine",
        NormalisableRange<float> (-100.f, 100.f, 1.f), 0.f, cents));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "submix", 1 }, "Mix",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "subwidth", 1 }, "Width",
        NormalisableRange<float> (0.05f, 0.95f, 0.01f), 0.5f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "bits", 1 }, "Bits",
        NormalisableRange<float> (2.f, 16.f, 1.f), 16.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "hold", 1 }, "Hold",
        NormalisableRange<float> (1.f, 80.f, 1.f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "glitch", 1 }, "Glitch",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "crushmix", 1 }, "Wet",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "compthresh", 1 }, "Threshold",
        NormalisableRange<float> (-40.f, 0.f, 0.1f), 0.f, decibels));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "compratio", 1 }, "Ratio",
        NormalisableRange<float> (1.f, 20.f, 0.1f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "compgain", 1 }, "Makeup",
        NormalisableRange<float> (0.f, 24.f, 0.1f), 0.f, decibels));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "limit", 1 }, "Limit",
        NormalisableRange<float> (-24.f, 0.f, 0.1f), -1.f, decibels));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "limitdist", 1 }, "Dist",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "slow", 1 }, "Speed",
        NormalisableRange<float> (0.25f, 1.25f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "slowsync", 1 }, "Sync", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "slowretrig", 1 }, "Speed retrig", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "pitchfix", 1 }, "Pitch fix", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "scatter", 1 }, "Scatter",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "seed", 1 }, "Seed",
        NormalisableRange<float> (0.f, 42.f, 1.f), 1.f, live));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "scatmode", 1 },
        "Pattern",
        StringArray { "Warble", "Repeat" },
        0,
        choice));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "steps", 1 }, "Steps",
        NormalisableRange<float> (1.f, 16.f, 1.f), 5.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "scatglide", 1 }, "Warble glide",
        NormalisableRange<float> (0.f, 2.f, 0.001f), 0.f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "wobrate", 1 }, "Warble rate",
        NormalisableRange<float> (0.1f, 30.f, 0.01f, 0.35f), 1.2f,
        AudioParameterFloatAttributes().withAutomatable (true).withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "srcglide", 1 }, "Source glide",
        NormalisableRange<float> (0.f, 2.f, 0.001f), 0.f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "delay", 1 }, "Delay",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f, seconds));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "delaysync", 1 }, "Delay sync", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "delayretrig", 1 }, "Delay retrig", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "feedback", 1 }, "Feedback",
        NormalisableRange<float> (0.f, 0.99f, 0.001f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "echo", 1 }, "Echo",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "filttype", 1 },
        "Type",
        StringArray { "Off", "Lowpass", "Highpass", "Bandpass", "Notch", "Formant", "Resonator" },
        0,
        choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "filtwhere", 1 },
        "Where",
        StringArray { "After generation", "Before output" },
        0,
        choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "fxorder", 1 },
        "Order",
        StringArray { "Crush, filter, delay, comp",
                       "Filter, crush, delay, comp",
                       "Filter, delay, crush, comp",
                       "Crush, filter, comp, delay",
                       "Crush, delay, filter, comp",
                       "Crush, filter, delay, tape, comp" },
        0,
        choice));
    const auto slots = StringArray { "1", "2", "3", "4", "5", "6" };
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "tapeslot", 1 }, "Tape place", slots, 0, choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "crushslot", 1 }, "Crush place", slots, 2, choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "filtslot", 1 }, "Filter place", slots, 1, choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "delayslot", 1 }, "Delay place", slots, 3, choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "revslot", 1 }, "Reverb place", slots, 4, choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "compslot", 1 }, "Compress place", slots, 5, choice));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "revsize", 1 }, "Size",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.45f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "revdamp", 1 }, "Damp",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.4f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "revmix", 1 }, "Mix",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    const auto hertz = AudioParameterFloatAttributes().withAutomatable (true).withLabel ("Hz");
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "filtcut", 1 }, "Cut",
        NormalisableRange<float> (40.f, 16000.f, 1.f, 0.3f), 4000.f, hertz));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "filtq", 1 }, "Reso",
        NormalisableRange<float> (0.3f, 8.f, 0.01f), 0.7f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "formshift", 1 }, "Shift",
        NormalisableRange<float> (0.5f, 2.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "formspread", 1 }, "Spread",
        NormalisableRange<float> (0.25f, 2.5f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "formbreath", 1 }, "Breath",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "resodamp", 1 }, "Damp",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "resotune", 1 }, "Tune",
        NormalisableRange<float> (-24.f, 24.f, 0.1f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "resostack", 1 }, "Stack",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    const auto timeRange = NormalisableRange<float> (0.001f, 4.f, 0.001f, 0.35f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "fattack", 1 }, "Filter attack",
        timeRange, 0.01f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "fdecay", 1 }, "Filter decay",
        timeRange, 0.18f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "fsustain", 1 }, "Filter sustain",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "frelease", 1 }, "Filter release",
        timeRange, 0.2f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "fenv", 1 }, "Filter envelope",
        NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f, live));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "fattacksync", 1 }, "Filter attack sync", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "fdecaysync", 1 }, "Filter decay sync", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "freleasesync", 1 }, "Filter release sync", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "samspeed", 1 }, "Speed",
        NormalisableRange<float> (1.f, 255.f, 1.f), 72.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "sampitch", 1 }, "Pitch",
        NormalisableRange<float> (1.f, 255.f, 1.f), 64.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "sammouth", 1 }, "Mouth",
        NormalisableRange<float> (0.f, 255.f, 1.f), 128.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "samthroat", 1 }, "Throat",
        NormalisableRange<float> (0.f, 255.f, 1.f), 128.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "samslow", 1 }, "Sam slow",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "espeakrate", 1 }, "Rate",
        NormalisableRange<float> (80.f, 450.f, 1.f), 175.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "espeakpitch", 1 }, "Pitch",
        NormalisableRange<float> (0.f, 99.f, 1.f), 50.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "espeakrange", 1 }, "Range",
        NormalisableRange<float> (0.f, 99.f, 1.f), 50.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "espeakgap", 1 }, "Gap",
        NormalisableRange<float> (0.f, 20.f, 1.f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "espeakvoice", 1 }, "Voice",
        NormalisableRange<float> (0.f, 11.f, 1.f), 0.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "level", 1 }, "Level",
        NormalisableRange<float> (-18.f, 12.f, 0.1f), 0.8f, decibels));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "srclevel", 1 }, "Source",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.5f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "sublevel", 1 }, "Sub",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.5f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "crushlevel", 1 }, "Crush",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "filtmix", 1 }, "Filter",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "delaylevel", 1 }, "Delay",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "compmix", 1 }, "Compress",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack",
        timeRange, 0.2f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "decay", 1 }, "Decay",
        timeRange, 0.2f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "sustain", 1 }, "Sustain",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.85f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release",
        timeRange, 0.2f, seconds));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "attacksync", 1 }, "Attack sync", true,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "decaysync", 1 }, "Decay sync", true,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "releasesync", 1 }, "Release sync", true,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "subattack", 1 }, "Sub attack",
        timeRange, 1.759f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "subdecay", 1 }, "Sub decay",
        timeRange, 0.12f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "subsustain", 1 }, "Sub sustain",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.85f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "subrelease", 1 }, "Sub release",
        timeRange, 0.28f, seconds));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "subattacksync", 1 }, "Sub attack sync", true,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "subdecaysync", 1 }, "Sub decay sync", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "subreleasesync", 1 }, "Sub release sync", false,
        AudioParameterBoolAttributes().withAutomatable (true)));
    const auto lfoRate = NormalisableRange<float> (0.05f, 20.f, 0.001f, 0.4f);
    const float lfoRateDefault[4] = { 2.f, 0.5f, 0.25f, 0.1f };
    for (int i = 1; i <= 4; ++i)
    {
        const auto n = String (i);
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { "lfo" + n + "shape", 1 },
            "LFO " + n + " shape",
            StringArray { "Sine", "Triangle", "Saw up", "Saw down", "Square", "Hold" },
            0,
            choice));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "lfo" + n + "rate", 1 }, "LFO " + n + " rate",
            lfoRate, lfoRateDefault[i - 1], hertz));
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { "lfo" + n + "sync", 1 }, "LFO " + n + " sync", false,
            AudioParameterBoolAttributes().withAutomatable (true)));
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { "lfo" + n + "retrig", 1 }, "LFO " + n + " retrig", false,
            AudioParameterBoolAttributes().withAutomatable (true)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "lfo" + n + "depth", 1 }, "LFO " + n + " depth",
            NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f, live));
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { "lfo" + n + "dest", 1 },
            "LFO " + n + " dest",
            StringArray { "Off", "Cut", "Reso", "Delay", "Echo", "Wet", "Glitch", "Speed", "Level",
                           "Bits", "Hold", "Feedback", "Scatter", "Tune", "Mix",
                           "Shift", "Spread", "Breath", "Damp", "Ring", "Stack",
                           "Attack", "Decay", "Sustain", "Release", "Filter env",
                           "Speed", "Pitch", "Seed",
                           "Filt attack", "Filt decay", "Filt sustain", "Filt release",
                           "Steps", "Width", "Sam slow" },
            0,
            choice));
    }
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClicksProcessor();
}

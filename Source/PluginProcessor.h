#pragma once

#include "ClicksEngine.h"
#include <array>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>

class ClicksProcessor : public juce::AudioProcessor,
                        private juce::AsyncUpdater
{
public:
    ClicksProcessor();
    ~ClicksProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NullAtomo"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Delay runs to 16 s with feedback, plus reverb. Bounces keep that tail.
    double getTailLengthSeconds() const override { return 20.0; }

    // Shipped presets. Program 0 is init. The host and the panel arrows share them.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    // Message thread. Sets every knob, the word, and the preset name.
    void applyFactoryPreset (int index);

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> meter { 0.f };

    int copyScope (float* dst, int n) const { return engine.copyScope (dst, n); }
    int copySamScope (float* dst, int n) const { return engine.copySamScope (dst, n); }
    float bassHz() const { return engine.bassHz(); }
    float lfoAmount (int index) const { return engine.lfoAmount (index); }
    double engineSampleRate() const { return engine.engineSampleRate(); }

    void setSamText (const juce::String& text);
    juce::String samText() const;
    int samRevision() const { return samRev.load (std::memory_order_acquire); }
    void requestSpeech();
    bool takeSpeechRequest();
    bool samNeedsRender() const;
    juce::String lockSpeech (bool announce = true);
    void replacePluginState (const juce::XmlElement& xml);

    void armMidiLearn (const juce::String& paramId);
    void cancelMidiLearn();
    bool isMidiLearning() const;
    juce::String midiLearnTarget() const;
    juce::String paramNameForId (const juce::String& paramId) const;
    void clearMidiMappingForParam (const juce::String& paramId);
    int midiCcForParam (const juce::String& paramId) const;
    int takeMidiLearnNotice (juce::String& paramIdOut);

private:
    // Every id the audio thread reads. Looked up once, then read by index.
#define NG_TAPS(X) \
    X(wave) X(width) X(bits) X(hold) X(glitch) X(crushmix) X(slow) X(pitchfix) X(slowsync) \
    X(scatter) X(seed) X(scatmode) X(steps) X(scatglide) X(wobrate) X(srcglide) \
    X(delay) X(delaysync) X(feedback) X(echo) X(poly) \
    X(attack) X(attacksync) X(decay) X(decaysync) X(sustain) X(release) X(releasesync) \
    X(submode) X(subwave) X(subtune) X(subfine) X(submix) X(subwidth) \
    X(subattack) X(subattacksync) X(subdecay) X(subdecaysync) X(subsustain) X(subrelease) X(subreleasesync) \
    X(filttype) X(fxorder) \
    X(tapeslot) X(crushslot) X(filtslot) X(delayslot) X(revslot) X(compslot) \
    X(revsize) X(revdamp) X(revmix) X(filtcut) X(filtq) \
    X(formshift) X(formspread) X(formbreath) X(resodamp) X(resotune) X(resostack) \
    X(fattack) X(fattacksync) X(fdecay) X(fdecaysync) X(fsustain) X(frelease) X(freleasesync) X(fenv) \
    X(compthresh) X(compratio) X(compgain) X(limit) X(limitdist) X(samslow) \
    X(level) X(srclevel) X(sublevel) X(crushlevel) X(filtmix) X(delaylevel) X(compmix) \
    X(lfo1shape) X(lfo1rate) X(lfo1sync) X(lfo1depth) X(lfo1dest) \
    X(lfo2shape) X(lfo2rate) X(lfo2sync) X(lfo2depth) X(lfo2dest) \
    X(lfo3shape) X(lfo3rate) X(lfo3sync) X(lfo3depth) X(lfo3dest) \
    X(lfo4shape) X(lfo4rate) X(lfo4sync) X(lfo4depth) X(lfo4dest) \
    X(lfo1retrig) X(lfo2retrig) X(lfo3retrig) X(lfo4retrig) X(delayretrig) X(slowretrig) \
    X(filt2type) X(filt2cut) X(filt2q) \
    X(form2shift) X(form2spread) X(form2breath) X(reso2damp) X(reso2tune) X(reso2stack) \
    X(f2attack) X(f2attacksync) X(f2decay) X(f2decaysync) X(f2sustain) X(f2release) X(f2releasesync) X(f2env) \
    X(filt2mix) \
    X(suborder) X(subcut) X(subreso)
    enum Tap
    {
#define NG_TAP_ENUM(n) tap_##n,
        NG_TAPS (NG_TAP_ENUM)
#undef NG_TAP_ENUM
        tapCount
    };
    struct TapRef
    {
        std::atomic<float>* raw = nullptr;
        juce::RangedAudioParameter* param = nullptr;
    };
    TapRef taps[tapCount];
    void bindTaps();
    float tap (int id) const { return taps[id].raw->load(); }

    void applyMidiController (juce::RangedAudioParameter* param, int value);
    void loadMidiMapFromState();
    void saveMidiMapToState();
    void rebuildCcTargets();
    bool handleMidiController (int cc, int value);
    ClickParams readParams() const;
    void renderRange (juce::AudioBuffer<float>& buffer, int start, int end);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;

    ClicksEngine engine;
    std::atomic<int> currentProgram { 0 };
    std::atomic<int> wantedProgram { -1 };
    mutable juce::SpinLock samLock;
    char samTextBuf[48] {};
    std::atomic<int> samRev { 0 };
    std::atomic<bool> speechRequest { false };
    int samLiveSpeed = -1;
    int samLivePitch = -1;
    int samLiveMouth = -1;
    int samLiveThroat = -1;
    int speechLiveKind = -1;
    int speakLiveRate = -1;
    int speakLivePitch = -1;
    int speakLiveRange = -1;
    int speakLiveGap = -1;
    int speakLiveVoice = -1;
    juce::String samLiveWord;

    juce::CriticalSection midiMapLock;
    std::array<juce::String, 128> midiCcToParam {};
    // Audio thread view of the map. Null means that CC is free.
    std::array<std::atomic<juce::RangedAudioParameter*>, 128> ccTarget {};
    juce::String pendingLearnParam;
    std::atomic<bool> learnArmed { false };
    std::atomic<bool> mapDirty { false };
    std::atomic<int> learnNotifyCc { -1 };
    juce::String learnNotifyParam;
    struct SyncRefresh;
    std::unique_ptr<SyncRefresh> syncRefresh;
    std::atomic<bool> syncInfoDirty { false };
    void watchDivisions();

    // Renders Sam and eSpeak while the panel is closed. The editor timer
    // is not running then, and Reason does not pump it until the page opens.
    struct SpeechPump;
    std::unique_ptr<SpeechPump> speechPump;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClicksProcessor)
};

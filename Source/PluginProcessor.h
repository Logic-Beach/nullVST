#pragma once

#include "ClicksEngine.h"
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

class ClicksProcessor : public juce::AudioProcessor
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

    const juce::String getName() const override { return "NULL GLITCH"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

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
    void applyMidiController (int cc, int value);
    void loadMidiMapFromState();
    void saveMidiMapToState();
    bool handleMidiController (int cc, int value);
    ClickParams readParams() const;
    void renderRange (juce::AudioBuffer<float>& buffer, int start, int end);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ClicksEngine engine;
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
    juce::String pendingLearnParam;
    std::atomic<int> learnNotifyCc { -1 };
    juce::String learnNotifyParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClicksProcessor)
};

#include "PluginProcessor.h"
#include "PluginEditor.h"

ClicksProcessor::ClicksProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

ClicksProcessor::~ClicksProcessor() = default;

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
    params.voice = (int) apvts.getRawParameterValue ("voice")->load();
    params.bits = apvts.getRawParameterValue ("bits")->load();
    params.hold = apvts.getRawParameterValue ("hold")->load();
    params.glitch = apvts.getRawParameterValue ("glitch")->load();
    params.slow = apvts.getRawParameterValue ("slow")->load();
    params.scatter = apvts.getRawParameterValue ("scatter")->load();
    params.seed = apvts.getRawParameterValue ("seed")->load();
    params.doublet = apvts.getRawParameterValue ("double")->load();
    params.delaySec = apvts.getRawParameterValue ("delay")->load();
    params.feedback = apvts.getRawParameterValue ("feedback")->load();
    params.echo = apvts.getRawParameterValue ("echo")->load();
    params.poly = (int) apvts.getRawParameterValue ("poly")->load();
    params.attack = apvts.getRawParameterValue ("attack")->load();
    params.decay = apvts.getRawParameterValue ("decay")->load();
    params.sustain = apvts.getRawParameterValue ("sustain")->load();
    params.release = apvts.getRawParameterValue ("release")->load();
    const float db = apvts.getRawParameterValue ("level")->load();
    params.level = std::pow (10.f, db / 20.f);
    return params;
}

void ClicksProcessor::renderRange (juce::AudioBuffer<float>& buffer, int start, int end)
{
    const int n = end - start;
    if (n <= 0)
        return;
    float* chans[2] = { nullptr, nullptr };
    const int nc = std::min (buffer.getNumChannels(), 2);
    for (int c = 0; c < nc; ++c)
        chans[c] = buffer.getWritePointer (c) + start;
    engine.process (chans, nc, n);
}

void ClicksProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    keyboard.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
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

void ClicksProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ClicksProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
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
        ParameterID { "voice", 1 },
        "Voice",
        StringArray { "Random", "Blip", "Chit", "Both" },
        0,
        choice));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "poly", 1 },
        "Voices",
        StringArray { "Mono", "Poly" },
        1,
        choice));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "bits", 1 }, "Bits",
        NormalisableRange<float> (2.f, 8.f, 1.f), 4.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "hold", 1 }, "Hold",
        NormalisableRange<float> (1.f, 40.f, 1.f), 14.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "glitch", 1 }, "Glitch",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "slow", 1 }, "Slow",
        NormalisableRange<float> (0.25f, 1.25f, 0.001f), 0.46f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "scatter", 1 }, "Scatter",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "seed", 1 }, "Seed",
        NormalisableRange<float> (0.f, 999.f, 1.f), 1.f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "double", 1 }, "Double",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.4f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "delay", 1 }, "Delay",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "feedback", 1 }, "Feedback",
        NormalisableRange<float> (0.f, 0.85f, 0.001f), 0.3f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "echo", 1 }, "Echo",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.5f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "level", 1 }, "Level",
        NormalisableRange<float> (-18.f, 12.f, 0.1f), 0.f, decibels));
    const auto timeRange = NormalisableRange<float> (0.001f, 4.f, 0.001f, 0.35f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack",
        timeRange, 0.01f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "decay", 1 }, "Decay",
        timeRange, 0.12f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "sustain", 1 }, "Sustain",
        NormalisableRange<float> (0.f, 1.f, 0.01f), 0.85f, live));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release",
        timeRange, 0.28f, seconds));
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClicksProcessor();
}

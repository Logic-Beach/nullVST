#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ClicksEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit ClicksEditor (ClicksProcessor&);
    ~ClicksEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::Font chicagoFont (float height) const;

    class Look : public juce::LookAndFeel_V4
    {
    public:
        Look();
        juce::Typeface::Ptr face;
        juce::Font faceFont (float height) const;

        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getPopupMenuFont() override;
        juce::Font getSliderPopupFont (juce::Slider&) override;

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::unique_ptr<Knob> addKnob (const juce::String& id, const juce::String& name);
    void placeRow (std::vector<std::unique_ptr<Knob>>& knobs, int begin, int count, int columns,
                   juce::Rectangle<int> row);

    ClicksProcessor& clicks;
    Look look;
    juce::Image field;
    juce::Image grain;

    std::vector<std::unique_ptr<Knob>> generator;
    std::vector<std::unique_ptr<Knob>> output;

    juce::ComboBox voiceBox;
    juce::ComboBox polyBox;
    juce::Label voiceLabel;
    juce::Label polyLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> polyAttachment;

    juce::MidiKeyboardComponent keyboard;
    juce::Rectangle<int> genPanel;
    juce::Rectangle<int> outPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClicksEditor)
};

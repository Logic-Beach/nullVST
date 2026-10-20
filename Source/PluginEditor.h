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
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void paintFace (juce::Graphics&);
    void paintShadows (juce::Graphics&);
    void tryMidiLearnFrom (juce::Component* target, const juce::MouseEvent& event);
    void showNote (const juce::String&);
    void refreshScope();
    void initPreset();
    void savePreset();
    void loadPreset();
    void refreshSam();
    void refreshFilter();
    void refreshSource();
    void refreshCrush();
    void refreshLfo();
    void animateLfos();
    void refreshScatter();
    void routeSlot (int index);
    struct TempoHold { bool on = false; float saved = 0.f; };
    juce::File presetFolder() const;
    juce::Font chicagoFont (float height) const;

    class Look : public juce::LookAndFeel_V4
    {
    public:
        Look();
        juce::Typeface::Ptr face;
        juce::Font faceFont (float height) const;

        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getPopupMenuFont() override;
        void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                        int standardMenuItemHeight,
                                        int& idealWidth, int& idealHeight) override;
        juce::Font getSliderPopupFont (juce::Slider&) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
        void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;

        juce::Image knob[9];
        juce::Image faderTrack;
        juce::Image faderCap;
        juce::Image barFill;
        juce::Image switchArt;
        juce::Image wave[5];
        juce::Image tapeDeck;
        juce::Image panelMain;
        juce::Image panelOut;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        juce::Label hint;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::unique_ptr<Knob> addKnob (const juce::String& id, const juce::String& name,
                                   const juce::String& hint, const juce::String& note);
    std::unique_ptr<Knob> addFader (const juce::String& id, const juce::String& name,
                                    const juce::String& hint, const juce::String& note);
    void placeRow (std::vector<std::unique_ptr<Knob>>& knobs, int begin, int count, int columns,
                   juce::Rectangle<int> row);
    void snapDivision (Knob& knob, const char* id, const char* syncId,
                       TempoHold& hold, const juce::String& freeHint);
    Knob* knobForDest (int dest);

    ClicksProcessor& clicks;
    Look look;
    juce::TooltipWindow tips;
    juce::Image field;
    juce::Image grain;
    juce::Image backdrop;
    void bakeBackdrop();
    juce::Image mark;
    juce::Image logoPlate;
    juce::Image botPlate;
    juce::Image botWash;

    std::vector<std::unique_ptr<Knob>> generator;
    std::vector<std::unique_ptr<Knob>> sam;
    std::vector<std::unique_ptr<Knob>> speak;
    std::vector<std::unique_ptr<Knob>> filter;
    std::vector<std::unique_ptr<Knob>> filter2;
    std::vector<std::unique_ptr<Knob>> output;
    std::vector<std::unique_ptr<Knob>> env;
    std::vector<std::unique_ptr<Knob>> senv;
    std::vector<std::unique_ptr<Knob>> sublp;
    std::vector<std::unique_ptr<Knob>> fenv;
    std::vector<std::unique_ptr<Knob>> fenv2;
    std::vector<std::unique_ptr<Knob>> comp;
    std::vector<std::unique_ptr<Knob>> reverb;
    std::vector<std::unique_ptr<Knob>> wobble;
    std::vector<std::unique_ptr<Knob>> mix;
    std::vector<std::unique_ptr<Knob>> lfo;

    juce::TextButton initButton;
    juce::TextButton saveButton;
    juce::TextButton loadButton;
    juce::TextButton prevButton;
    juce::TextButton nextButton;
    juce::Label presetBox;
    juce::String presetName;
    void showPreset();
    void stepPreset (int delta);
    void choosePreset (int index);
    void showPresetMenu();
    juce::TextButton lockButton;
    juce::ToggleButton syncTick;
    juce::ToggleButton slowRetrig;
    juce::ToggleButton pitchTick;
    juce::ToggleButton delaySync;
    juce::ToggleButton delayRetrig;
    juce::ToggleButton attackSync;
    juce::ToggleButton decaySync;
    juce::ToggleButton releaseSync;
    juce::ToggleButton lfoSync[4];
    juce::ToggleButton lfoRetrig[4];
    juce::ToggleButton fAttackSync;
    juce::ToggleButton fDecaySync;
    juce::ToggleButton fReleaseSync;
    juce::ToggleButton f2AttackSync;
    juce::ToggleButton f2DecaySync;
    juce::ToggleButton f2ReleaseSync;
    juce::ToggleButton subOrderTick;
    juce::ToggleButton subAttackSync;
    juce::ToggleButton subDecaySync;
    juce::ToggleButton subReleaseSync;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slowRetrigAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> delayRetrigAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pitchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> delaySyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attackSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> decaySyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> releaseSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoSyncAttachment[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoRetrigAttachment[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fAttackSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fDecaySyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fReleaseSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> f2AttackSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> f2DecaySyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> f2ReleaseSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> subOrderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> subAttackSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> subDecaySyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> subReleaseSyncAttachment;
    juce::Label sayLabel;
    juce::TextEditor sayBox;
    std::unique_ptr<juce::FileChooser> chooser;
    int samSeen = -1;
    juce::uint32 lastSamLive = 0;
    bool tempoOn = false;
    float freeSlow = 1.f;
    TempoHold delayHold, attackHold, decayHold, releaseHold, lfoHold[4];
    TempoHold fAttackHold, fDecayHold, fReleaseHold;
    TempoHold f2AttackHold, f2DecayHold, f2ReleaseHold;
    TempoHold subAttackHold, subDecayHold, subReleaseHold;

    juce::ComboBox waveBox;
    juce::ComboBox polyBox;
    juce::ComboBox scatBox;
    juce::ComboBox subBox;
    juce::ComboBox subWaveBox;
    juce::ComboBox filtTypeBox;
    juce::ComboBox filt2TypeBox;
    juce::TextButton filtTab[2];
    int filtPage = 0;
    juce::ComboBox fxOrderBox;
    juce::ComboBox lfoShapeBox[4];
    juce::ComboBox lfoDestBox[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> polyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scatAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subWaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filtTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filt2TypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fxOrderAttachment;
    juce::ComboBox slotBox[6];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slotAttachment[6];
    bool slotGuard = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttachment[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoDestAttachment[4];
    juce::TextButton lfoTab[4];
    int lfoPage = 0;
    juce::Label waveLabel;
    juce::Label voiceLabel;
    juce::Label polyLabel;
    juce::Label scatLabel;
    juce::Label subLabel;
    juce::Label subWaveLabel;
    juce::Label filtTypeLabel;
    juce::Label filt2TypeLabel;
    juce::Label fxOrderLabel;
    juce::Label lfoShapeLabel[4];
    juce::Label lfoDestLabel[4];
    juce::Label lfoTitle[4];
    juce::Rectangle<int> tapeFrame;
    juce::Rectangle<int> subFrame;
    juce::Rectangle<int> crushFrame;
    juce::Rectangle<int> filtFrame;
    juce::Rectangle<int> formFrame;
    juce::Rectangle<int> ringFrame;
    juce::Rectangle<int> delayFrame;
    juce::Rectangle<int> revFrame;
    juce::Rectangle<int> compFrame;
    juce::Rectangle<int> meterBounds;
    juce::Rectangle<int> tapeArt;
    juce::Rectangle<int> wavePlate;
    juce::Rectangle<int> subWavePlate;
    juce::Rectangle<int> envHoldPlate;
    juce::Rectangle<int> subHoldPlate;
    juce::Rectangle<int> filtHoldPlate;
    juce::Label outMeter;
    int filtTypeShown = -1;
    int waveShown = -1;
    float meterShown = 0.f;
    float meterHold = 0.f;
    int meterHoldAge = 0;

    juce::MidiKeyboardComponent keyboard;
    juce::Rectangle<int> genPanel;
    juce::Rectangle<int> filterPanel;
    juce::Rectangle<int> outPanel;
    juce::Rectangle<int> lfoPanel;
    juce::Rectangle<int> scopePanel;
    juce::Rectangle<int> botPanel;
    juce::Image bot;
    juce::Rectangle<float> botImageBounds() const;
    juce::Rectangle<int> waveTag;
    juce::Rectangle<int> playTag;
    juce::Rectangle<int> lfoTag;
    juce::Rectangle<int> samTag;
    juce::Rectangle<int> crushTag;
    juce::Rectangle<int> scatTag;
    juce::Rectangle<int> envTag;
    juce::Rectangle<int> subEnvTag;
    juce::Rectangle<int> subTag;
    juce::Rectangle<int> filtTag;
    juce::Rectangle<int> formantTag;
    juce::Rectangle<int> ringTag;
    juce::Rectangle<int> filtEnvTag;
    juce::Rectangle<int> delayTag;
    juce::Rectangle<int> revTag;
    juce::Rectangle<int> compTag;
    juce::Rectangle<int> outTag;
    juce::Rectangle<int> titleBand;
    juce::String status;
    juce::uint32 statusUntil = 0;
    juce::uint32 learnUntil = 0;
    std::vector<float> scopeTrace;
    std::vector<float> samTrace;
    double scopePeriod = 0;
    double samPeriod = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClicksEditor)
};

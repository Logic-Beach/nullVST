#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour kPaper (0xfff3eee4);
    const juce::Colour kPencil (0xff241e18);
    const juce::Colour kInk (0xff3c342c);

    float noise (uint32_t s)
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return ((s >> 8) & 255u) / 255.f;
    }

    juce::Path wobbleRect (juce::Rectangle<float> r, uint32_t salt)
    {
        juce::Path path;
        const int steps = 8;
        auto edge = [&] (float x0, float y0, float x1, float y1, int id, bool first)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float len = std::sqrt (dx * dx + dy * dy) + 0.001f;
            for (int i = 0; i <= steps; ++i)
            {
                const float t = (float) i / (float) steps;
                float x = x0 + dx * t;
                float y = y0 + dy * t;
                const float wob = (noise (salt + (uint32_t) (id * 48 + i) * 19u) - 0.5f) * 3.4f;
                x += -dy / len * wob;
                y += dx / len * wob;
                if (first && i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }
        };
        edge (r.getX(), r.getY(), r.getRight(), r.getY(), 0, true);
        edge (r.getRight(), r.getY(), r.getRight(), r.getBottom(), 1, false);
        edge (r.getRight(), r.getBottom(), r.getX(), r.getBottom(), 2, false);
        edge (r.getX(), r.getBottom(), r.getX(), r.getY(), 3, false);
        path.closeSubPath();
        return path;
    }

    juce::Path wobbleDisc (juce::Point<float> c, float radius, uint32_t salt)
    {
        juce::Path path;
        const int n = 28;
        for (int i = 0; i <= n; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) n;
            const float wob = 0.90f + noise (salt + (uint32_t) i * 23u) * 0.16f;
            const float x = c.x + std::cos (a) * radius * wob;
            const float y = c.y + std::sin (a) * radius * wob * 0.96f;
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        path.closeSubPath();
        return path;
    }
}

ClicksEditor::Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, kPaper);
    setColour (juce::PopupMenu::textColourId, kPencil);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffddd4c4));
    setColour (juce::PopupMenu::highlightedTextColourId, kPencil);
}

juce::Font ClicksEditor::Look::faceFont (float height) const
{
    if (face != nullptr)
        return juce::Font (juce::FontOptions (face).withHeight (height));
    return juce::Font (juce::FontOptions (height));
}

juce::Font ClicksEditor::Look::getComboBoxFont (juce::ComboBox&) { return faceFont (16.f); }
juce::Font ClicksEditor::Look::getLabelFont (juce::Label&)       { return faceFont (15.f); }
juce::Font ClicksEditor::Look::getPopupMenuFont()                { return faceFont (16.f); }
juce::Font ClicksEditor::Look::getSliderPopupFont (juce::Slider&) { return faceFont (13.f); }

void ClicksEditor::Look::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider& slider)
{
    const float side = (float) juce::jmin (width, height);
    const auto centre = juce::Point<float> ((float) x + (float) width * 0.5f,
                                             (float) y + (float) height * 0.5f);
    const float radius = side * 0.38f;
    const uint32_t salt = (uint32_t) slider.getName().hashCode();
    const auto disc = wobbleDisc (centre, radius, salt);

    g.setColour (kPaper);
    g.fillPath (disc);

    g.saveState();
    g.reduceClipRegion (disc);
    g.setColour (kPencil.withAlpha (0.16f));
    for (float yy = centre.y - radius; yy < centre.y + radius; yy += 3.1f)
        g.drawLine (centre.x - radius, yy, centre.x + radius, yy - 7.f, 0.7f);
    g.setColour (kPencil.withAlpha (0.10f));
    for (float xx = centre.x - radius; xx < centre.x + radius; xx += 3.4f)
        g.drawLine (xx, centre.y - radius, xx - 6.f, centre.y + radius, 0.6f);
    g.restoreState();

    g.setColour (kPencil.withAlpha (0.45f));
    auto ghost = disc;
    ghost.applyTransform (juce::AffineTransform::translation (0.8f, 0.7f));
    g.strokePath (ghost, juce::PathStrokeType (1.3f));
    g.setColour (kPencil);
    g.strokePath (disc, juce::PathStrokeType (1.7f));

    const int ticks = 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = (float) i / (float) (ticks - 1);
        const float ang = startAngle + t * (endAngle - startAngle);
        const float inner = radius + 3.f;
        const float outer = radius + (t <= sliderPos + 0.02f ? 8.5f : 5.5f);
        const float wob = (noise (salt + (uint32_t) i * 11u) - 0.5f) * 1.4f;
        juce::Line<float> mark (centre.x + std::cos (ang) * inner,
                                centre.y + std::sin (ang) * inner,
                                centre.x + std::cos (ang) * (outer + wob),
                                centre.y + std::sin (ang) * (outer + wob));
        g.setColour (t <= sliderPos + 0.001f ? kPencil : kPencil.withAlpha (0.35f));
        g.drawLine (mark, t <= sliderPos + 0.001f ? 1.6f : 1.0f);
    }

    const float ang = startAngle + sliderPos * (endAngle - startAngle);
    const float len = radius * 0.62f;
    juce::Path needle;
    needle.startNewSubPath (centre.x + std::cos (ang + 0.5f) * 2.2f,
                            centre.y + std::sin (ang + 0.5f) * 2.2f);
    needle.lineTo (centre.x + std::cos (ang) * len, centre.y + std::sin (ang) * len);
    g.setColour (kInk);
    g.strokePath (needle, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    g.setColour (kPencil);
    g.fillEllipse (centre.x - 2.4f, centre.y - 2.4f, 4.8f, 4.8f);
}

void ClicksEditor::Look::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<float> (1.f, 1.f, (float) width - 2.f, (float) height - 2.f);
    const auto card = wobbleRect (area, (uint32_t) box.getName().hashCode() + 9u);
    g.setColour (kPaper);
    g.fillPath (card);
    g.setColour (kPencil);
    g.strokePath (card, juce::PathStrokeType (1.6f));

    const float cx = (float) width - 16.f;
    const float cy = (float) height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 5.f, cy - 2.f);
    chevron.lineTo (cx, cy + 3.f);
    chevron.lineTo (cx + 5.f, cy - 2.f);
    g.strokePath (chevron, juce::PathStrokeType (1.5f));
}

void ClicksEditor::Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 28, box.getHeight());
    label.setFont (faceFont (16.f));
    label.setJustificationType (juce::Justification::centredLeft);
}

void ClicksEditor::Look::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (kPaper);
    g.setColour (kPencil);
    g.drawRect (0, 0, width, height, 2);
}

ClicksEditor::ClicksEditor (ClicksProcessor& synth)
    : AudioProcessorEditor (&synth),
      clicks (synth),
      keyboard (synth.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    look.face = juce::Typeface::createSystemTypefaceFor (BinaryData::Chicago12_otf,
                                                          BinaryData::Chicago12_otfSize);
    field = juce::ImageFileFormat::loadFrom (BinaryData::starfield_png, BinaryData::starfield_pngSize);

    grain = juce::Image (juce::Image::ARGB, 180, 180, true);
    {
        juce::Graphics gg (grain);
        for (int i = 0; i < 1100; ++i)
        {
            const uint32_t s = 0xB075u + (uint32_t) i * 97u;
            const float x = noise (s) * 180.f;
            const float y = noise (s * 3u + 1u) * 180.f;
            const float len = 2.f + noise (s * 5u) * 7.f;
            gg.setColour (kPencil.withAlpha (0.035f + noise (s * 9u) * 0.07f));
            gg.drawLine (x, y, x + len, y + (noise (s * 11u) - 0.5f) * 2.f, 0.7f);
        }
    }

    setLookAndFeel (&look);

    auto styleChoice = [this] (juce::ComboBox& box, juce::Label& caption, const juce::String& id, const juce::String& name)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (clicks.apvts.getParameter (id)))
            box.addItemList (choice->choices, 1);
        box.setJustificationType (juce::Justification::centredLeft);
        box.setLookAndFeel (&look);
        box.setColour (juce::ComboBox::textColourId, kPencil);
        addAndMakeVisible (box);
        caption.setText (name, juce::dontSendNotification);
        caption.setFont (chicagoFont (14.f));
        caption.setColour (juce::Label::textColourId, kPencil);
        caption.setJustificationType (juce::Justification::centredLeft);
        caption.setLookAndFeel (&look);
        addAndMakeVisible (caption);
    };
    styleChoice (voiceBox, voiceLabel, "voice", "voice");
    styleChoice (polyBox, polyLabel, "poly", "voices");
    voiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        clicks.apvts, "voice", voiceBox);
    polyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        clicks.apvts, "poly", polyBox);

    for (auto spec : { std::pair<const char*, const char*> { "seed", "seed" },
                       { "bits", "bits" },
                       { "hold", "hold" },
                       { "glitch", "glitch" },
                       { "slow", "slow" },
                       { "scatter", "scatter" },
                       { "double", "double" } })
        generator.push_back (addKnob (spec.first, spec.second));

    for (auto spec : { std::pair<const char*, const char*> { "attack", "attack" },
                       { "decay", "decay" },
                       { "sustain", "sustain" },
                       { "release", "release" },
                       { "delay", "delay" },
                       { "feedback", "feedback" },
                       { "echo", "echo" },
                       { "level", "level" } })
        output.push_back (addKnob (spec.first, spec.second));

    keyboard.setLookAndFeel (&look);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, kPaper);
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff2c261f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, kPencil);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x33221c16));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0x55221c16));
    keyboard.setAvailableRange (36, 84);
    keyboard.setScrollButtonsVisible (false);
    addAndMakeVisible (keyboard);

    setResizable (false, false);
    setSize (1080, 720);
    startTimerHz (12);
}

ClicksEditor::~ClicksEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    voiceBox.setLookAndFeel (nullptr);
    polyBox.setLookAndFeel (nullptr);
    keyboard.setLookAndFeel (nullptr);
    for (auto* group : { &generator, &output })
        for (auto& knob : *group)
            if (knob != nullptr)
            {
                knob->slider.setLookAndFeel (nullptr);
                knob->label.setLookAndFeel (nullptr);
            }
}

juce::Font ClicksEditor::chicagoFont (float height) const
{
    return look.faceFont (height);
}

std::unique_ptr<ClicksEditor::Knob> ClicksEditor::addKnob (const juce::String& id, const juce::String& name)
{
    auto knob = std::make_unique<Knob>();
    knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 22);
    knob->slider.setColour (juce::Slider::textBoxTextColourId, kPencil);
    knob->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    knob->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob->slider.setLookAndFeel (&look);
    knob->slider.setName (name);
    addAndMakeVisible (knob->slider);

    knob->label.setText (name, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setFont (chicagoFont (15.f));
    knob->label.setColour (juce::Label::textColourId, kPencil);
    knob->label.setLookAndFeel (&look);
    addAndMakeVisible (knob->label);

    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        clicks.apvts, id, knob->slider);
    return knob;
}

void ClicksEditor::placeRow (std::vector<std::unique_ptr<Knob>>& knobs, int begin, int count,
                             int columns, juce::Rectangle<int> row)
{
    if ((int) knobs.size() < begin + count || count <= 0 || columns <= 0)
        return;
    row.removeFromBottom (12);
    const int col = row.getWidth() / columns;
    for (int i = 0; i < count; ++i)
    {
        auto* knob = knobs[(size_t) (begin + i)].get();
        if (knob == nullptr)
            continue;
        auto cell = row.removeFromLeft (col).reduced (6, 0);
        knob->label.setBounds (cell.removeFromTop (20));
        cell.removeFromTop (4);
        knob->slider.setBounds (cell.removeFromTop (juce::jmin (cell.getHeight(), 132)));
    }
}

void ClicksEditor::paint (juce::Graphics& g)
{
    if (field.isValid())
        g.drawImage (field, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);
    else
        g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::black.withAlpha (0.38f));
    g.fillRect (0, 0, getWidth(), 72);

    g.setFont (chicagoFont (30.f));
    g.setColour (juce::Colours::white);
    g.drawText ("NULL GLITCH", 28, 14, 420, 36, juce::Justification::centredLeft);
    g.setFont (chicagoFont (16.f));
    g.setColour (juce::Colours::white.withAlpha (0.86f));
    g.drawText ("hold a key", getWidth() - 210, 22, 178, 22, juce::Justification::centredRight);

    const float level = juce::jlimit (0.f, 1.f, clicks.meter.load() * 2.4f);
    const int cells = 16;
    const int lit = (int) std::lround (level * (float) cells);
    for (int i = 0; i < cells; ++i)
    {
        g.setColour (i < lit ? juce::Colours::white : juce::Colours::white.withAlpha (0.35f));
        const float h = 10.f + (float) (i % 3) * 2.f;
        g.fillRect (250.f + (float) i * 12.f, 46.f - h * 0.2f, 7.f, h);
    }

    auto paper = [&] (juce::Rectangle<int> panel, uint32_t salt, const juce::String& title)
    {
        if (panel.isEmpty())
            return;
        const auto card = wobbleRect (panel.toFloat(), salt);
        g.setColour (kPaper.withAlpha (0.94f));
        g.fillPath (card);
        g.setTiledImageFill (grain, 0.f, 0.f, 0.85f);
        g.fillPath (card);
        auto ghost = card;
        ghost.applyTransform (juce::AffineTransform::translation (1.1f, 0.8f));
        g.setColour (kPencil.withAlpha (0.35f));
        g.strokePath (ghost, juce::PathStrokeType (1.2f));
        g.setColour (kPencil);
        g.strokePath (card, juce::PathStrokeType (1.8f));
        g.setFont (chicagoFont (22.f));
        g.drawText (title, panel.reduced (18, 8).removeFromTop (30), juce::Justification::centredLeft);
    };
    paper (genPanel, 0x61u, "generator");
    paper (outPanel, 0xC3u, "output");
}

void ClicksEditor::resized()
{
    auto area = getLocalBounds().reduced (18);
    area.removeFromTop (58);
    auto keys = area.removeFromBottom (100);
    area.removeFromBottom (14);

    auto left = area.removeFromLeft ((area.getWidth() - 16) / 2);
    area.removeFromLeft (16);
    genPanel = left;
    outPanel = area;
    keyboard.setKeyWidth (juce::jmax (12.f, (float) keys.getWidth() / 29.f));
    keyboard.setBounds (keys.reduced (4, 0));

    auto gen = genPanel.reduced (16, 12);
    gen.removeFromTop (34);
    auto menus = gen.removeFromTop (56);
    auto voiceCol = menus.removeFromLeft (menus.getWidth() / 2).reduced (6, 0);
    auto polyCol = menus.reduced (6, 0);
    voiceLabel.setBounds (voiceCol.removeFromTop (18));
    voiceBox.setBounds (voiceCol.reduced (0, 4));
    polyLabel.setBounds (polyCol.removeFromTop (18));
    polyBox.setBounds (polyCol.reduced (0, 4));

    const int genRow = gen.getHeight() / 2;
    placeRow (generator, 0, 4, 4, gen.removeFromTop (genRow));
    placeRow (generator, 4, 3, 4, gen);

    auto out = outPanel.reduced (16, 12);
    out.removeFromTop (34);
    const int outRow = out.getHeight() / 2;
    placeRow (output, 0, 4, 4, out.removeFromTop (outRow));
    placeRow (output, 4, 4, 4, out);
}

void ClicksEditor::timerCallback()
{
    repaint (240, 18, 220, 40);
}

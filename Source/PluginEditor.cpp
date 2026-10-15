#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour kPaper (0xfff3eee4);
    const juce::Colour kPencil (0xff241e18);
    const juce::Colour kInk (0xff3c342c);
    const juce::Colour kLfo1 (0xffe28a2a);
    const juce::Colour kLfo2 (0xff1f8f86);
    const juce::Colour kLfo3 (0xffc44ad0);
    const juce::Colour kLfo4 (0xff3a78e0);
    const juce::Colour kLfoCol[4] = { kLfo1, kLfo2, kLfo3, kLfo4 };
    constexpr int kDestN = 36;

    float noise (uint32_t s)
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return ((s >> 8) & 255u) / 255.f;
    }

    int knobFaceFor (const juce::String& id)
    {
        if (id == "slow")
            return 8;
        if (id == "scatter")
            return 6;
        if (id.startsWith ("sam") || id.startsWith ("espeak"))
            return 7;
        if (id == "bits" || id == "hold" || id == "glitch" || id == "crushmix"
            || id == "limit" || id == "limitdist")
            return 3;
        if (id == "delay" || id == "feedback" || id == "echo")
            return 2;
        if (id.startsWith ("rev"))
            return 5;
        if (id == "width" || id == "subwidth" || id == "srcglide" || id.endsWith ("depth"))
            return 4;
        if (id.startsWith ("filt") || id.startsWith ("form") || id.startsWith ("reso")
            || id == "subtune" || id == "submix" || id == "subfine"
            || id == "compthresh" || id == "compratio" || id == "compgain")
            return 1;
        return 0;
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

    // Blurred shadow, 2px down and to the right. The control itself is drawn
    // afterwards at full size and full opacity.
    constexpr int kShade = 2;

    bool shadowHits (const juce::Graphics& g, juce::Rectangle<int> area)
    {
        return area.intersects (g.getClipBounds().expanded (kShade * 2));
    }

    void castArt (juce::Graphics& g, const juce::Image& art, juce::Rectangle<float> area,
                  float rotation, bool flip)
    {
        auto bounds = area.getSmallestIntegerContainer();
        if (! art.isValid() || bounds.isEmpty() || ! shadowHits (g, bounds))
            return;
        const int pad = kShade;
        const int w = juce::jmax (1, bounds.getWidth());
        const int h = juce::jmax (1, bounds.getHeight());
        juce::Image stamp (juce::Image::ARGB, w + pad * 2, h + pad * 2, true);
        {
            juce::Graphics sg (stamp);
            auto dest = juce::Rectangle<float> ((float) pad, (float) pad, (float) w, (float) h);
            const float cx = dest.getCentreX();
            const float cy = dest.getCentreY();
            if (flip)
                sg.addTransform (juce::AffineTransform::scale (-1.f, 1.f, cx, cy));
            if (rotation != 0.f)
                sg.addTransform (juce::AffineTransform::rotation (rotation, cx, cy));
            sg.setColour (juce::Colours::white);
            sg.drawImage (art, dest, juce::RectanglePlacement::stretchToFit, true);
        }
        g.saveState();
        g.addTransform (juce::AffineTransform::translation ((float) bounds.getX() - (float) pad,
                                                           (float) bounds.getY() - (float) pad));
        juce::DropShadow (kPencil, kShade, { kShade, kShade }).drawForImage (g, stamp);
        g.restoreState();
    }

    void castRect (juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto bounds = area.getSmallestIntegerContainer();
        if (bounds.isEmpty() || ! shadowHits (g, bounds))
            return;
        juce::DropShadow (kPencil, kShade, { kShade, kShade }).drawForRectangle (g, bounds);
    }
}

ClicksEditor::Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, kPaper);
    setColour (juce::PopupMenu::textColourId, kPencil);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffddd4c4));
    setColour (juce::PopupMenu::highlightedTextColourId, kPencil);

    auto pict = [] (const void* data, int size)
    {
        return juce::ImageFileFormat::loadFrom (data, (size_t) size);
    };
    knob[0] = pict (BinaryData::knob1_png, BinaryData::knob1_pngSize);
    knob[1] = pict (BinaryData::knob2_png, BinaryData::knob2_pngSize);
    knob[2] = pict (BinaryData::knob3_png, BinaryData::knob3_pngSize);
    knob[3] = pict (BinaryData::knob4_png, BinaryData::knob4_pngSize);
    knob[4] = pict (BinaryData::knob5_png, BinaryData::knob5_pngSize);
    knob[5] = pict (BinaryData::knob6_png, BinaryData::knob6_pngSize);
    knob[6] = pict (BinaryData::knobbig_png, BinaryData::knobbig_pngSize);
    knob[7] = pict (BinaryData::knobface_png, BinaryData::knobface_pngSize);
    knob[8] = pict (BinaryData::knobtape_png, BinaryData::knobtape_pngSize);
    faderTrack = pict (BinaryData::faderbar_png, BinaryData::faderbar_pngSize);
    faderCap = pict (BinaryData::faderknob_png, BinaryData::faderknob_pngSize);
    barFill = pict (BinaryData::bar_png, BinaryData::bar_pngSize);
    switchArt = pict (BinaryData::switch_png, BinaryData::switch_pngSize);
    wave[0] = pict (BinaryData::wavesine_png, BinaryData::wavesine_pngSize);
    wave[1] = pict (BinaryData::wavesaw_png, BinaryData::wavesaw_pngSize);
    wave[2] = pict (BinaryData::wavesquare_png, BinaryData::wavesquare_pngSize);
    wave[3] = pict (BinaryData::wavetriangle_png, BinaryData::wavetriangle_pngSize);
    wave[4] = pict (BinaryData::wavenoise_png, BinaryData::wavenoise_pngSize);
    tapeDeck = pict (BinaryData::tape_png, BinaryData::tape_pngSize);
    panelMain = pict (BinaryData::backgroundmain_png, BinaryData::backgroundmain_pngSize);
    panelOut = pict (BinaryData::backgroundoutput_png, BinaryData::backgroundoutput_pngSize);

}

juce::Font ClicksEditor::Look::faceFont (float height) const
{
    if (face != nullptr)
        return juce::Font (juce::FontOptions (face).withHeight (height));
    return juce::Font (juce::FontOptions (height));
}

juce::Font ClicksEditor::Look::getLabelFont (juce::Label& label)
{
    const float h = label.getFont().getHeight();
    return faceFont (h > 1.f ? h : 15.f);
}
juce::Font ClicksEditor::Look::getPopupMenuFont()                { return faceFont (20.f); }

void ClicksEditor::Look::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                   int standardMenuItemHeight,
                                                   int& idealWidth, int& idealHeight)
{
    LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);
    if (! isSeparator)
        idealHeight = juce::jmax (idealHeight, 30);
}
juce::Font ClicksEditor::Look::getSliderPopupFont (juce::Slider&) { return faceFont (13.f); }
juce::Font ClicksEditor::Look::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return faceFont (buttonHeight > 4 ? (float) buttonHeight * 0.55f : 14.f);
}

juce::Font ClicksEditor::Look::getComboBoxFont (juce::ComboBox&)
{
    return faceFont (16.f);
}

void ClicksEditor::Look::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&)
{
    auto plate = juce::Rectangle<float> (0.f, 0.f, (float) width, (float) height);
    if (barFill.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (barFill, plate, juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        g.setColour (kPaper);
        g.fillRoundedRectangle (plate.reduced (0.5f), 3.f);
        g.setColour (kPencil);
        g.drawRoundedRectangle (plate.reduced (0.9f), 3.f, 1.2f);
    }
    juce::Path arrow;
    const float cx = (float) buttonX + (float) buttonW * 0.5f;
    const float cy = (float) buttonY + (float) buttonH * 0.5f;
    arrow.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
    g.setColour (kPencil);
    g.fillPath (arrow);
}

void ClicksEditor::Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

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

    const int lfoMark = (int) slider.getProperties().getWithDefault ("lfo", 0);
    for (int i = 0; i < 4; ++i)
    {
        if ((lfoMark & (1 << i)) == 0)
            continue;
        const float pad = 3.5f + (float) i * 3.2f;
        g.setColour (kLfoCol[i]);
        g.drawEllipse (centre.x - radius - pad, centre.y - radius - pad,
                       (radius + pad) * 2.f, (radius + pad) * 2.f, 2.2f);
    }

    const float lfoDrive = (float) slider.getProperties().getWithDefault ("lfoDrive", 0.0);
    sliderPos = juce::jlimit (0.f, 1.f, sliderPos + lfoDrive * 0.5f);

    const int art = (int) slider.getProperties().getWithDefault ("face", 0);
    if (art >= 0 && art < 9 && knob[art].isValid())
    {
        const float rot = (sliderPos - 0.5f) * (endAngle - startAngle);
        auto plate = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                         .withSizeKeepingCentre (side, side);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (rot, plate.getCentreX(), plate.getCentreY()));
        g.drawImage (knob[art], plate, juce::RectanglePlacement::stretchToFit);
        g.restoreState();
        return;
    }

    const int ticks = 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = (float) i / (float) (ticks - 1);
        const float ang = startAngle + t * (endAngle - startAngle);
        const float inner = radius + 3.f;
        const float outer = radius + (t <= sliderPos + 0.02f ? 8.5f : 5.5f);
        const float wob = (noise (salt + (uint32_t) i * 11u) - 0.5f) * 1.4f;
        juce::Line<float> tick (centre.x + std::cos (ang) * inner,
                                centre.y + std::sin (ang) * inner,
                                centre.x + std::cos (ang) * (outer + wob),
                                centre.y + std::sin (ang) * (outer + wob));
        g.setColour (t <= sliderPos + 0.001f ? kPencil : kPencil.withAlpha (0.35f));
        g.drawLine (tick, t <= sliderPos + 0.001f ? 1.6f : 1.0f);
    }

    // Knob art rotates with this angle.
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

void ClicksEditor::Look::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool vertical = style == juce::Slider::LinearVertical
                       || style == juce::Slider::LinearBarVertical;
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.f);

    const float lfoDrive = (float) slider.getProperties().getWithDefault ("lfoDrive", 0.0);
    if (std::abs (lfoDrive) > 1.0e-4f)
    {
        const float prop = (float) slider.valueToProportionOfLength (slider.getValue());
        const float shown = juce::jlimit (0.f, 1.f, prop + lfoDrive * 0.5f);
        const float travel = vertical ? area.getHeight() : area.getWidth();
        const float delta = (shown - prop) * travel;
        sliderPos += vertical ? -delta : delta;
    }

    const int lfoMark = (int) slider.getProperties().getWithDefault ("lfo", 0);
    juce::Colour capCol = kPencil;
    for (int i = 0; i < 4; ++i)
        if ((lfoMark & (1 << i)) != 0)
            capCol = kLfoCol[i];

    if (faderTrack.isValid() && faderCap.isValid())
    {
        auto track = vertical
            ? area.reduced (area.getWidth() * 0.22f, 0.f)
            : area.reduced (0.f, area.getHeight() * 0.28f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (faderTrack, track, juce::RectanglePlacement::stretchToFit);
        const float cap = vertical ? area.getWidth()
                                   : juce::jmin (area.getHeight() * 1.2f, 34.f);
        auto head = vertical
            ? juce::Rectangle<float> (area.getX(), sliderPos - cap * 0.5f, area.getWidth(), cap)
            : juce::Rectangle<float> (sliderPos - cap * 0.5f, area.getY(), cap, area.getHeight());
        g.drawImage (faderCap, head, juce::RectanglePlacement::stretchToFit);
        if (lfoMark != 0)
        {
            g.setColour (capCol);
            g.drawRect (head, 1.6f);
        }
        return;
    }

    const float trackW = 5.f;
    juce::Rectangle<float> track = vertical
        ? area.withSizeKeepingCentre (trackW, area.getHeight())
        : area.withSizeKeepingCentre (area.getWidth(), trackW);
    g.setColour (kPaper);
    g.fillRoundedRectangle (track, 2.f);
    g.setColour (kPencil);
    g.drawRoundedRectangle (track.reduced (0.4f), 2.f, 1.2f);

    const float thumb = vertical ? 14.f : 16.f;
    juce::Rectangle<float> head = vertical
        ? juce::Rectangle<float> (area.getCentreX() - 9.f, sliderPos - thumb * 0.5f, 18.f, thumb)
        : juce::Rectangle<float> (sliderPos - thumb * 0.5f, area.getCentreY() - 7.f, thumb, 14.f);
    g.setColour (capCol);
    g.fillRoundedRectangle (head, 2.f);
}

void ClicksEditor::Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool, bool)
{
    if ((int) button.getProperties().getWithDefault ("pair", 0) != 0)
    {
        auto area = button.getLocalBounds().toFloat().reduced (0.5f);
        auto left = area.withWidth (area.getWidth() * 0.5f);
        auto right = area.withTrimmedLeft (left.getWidth());
        const bool sync = button.getToggleState();
        if (switchArt.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.saveState();
            if (! sync)
                g.addTransform (juce::AffineTransform::scale (-1.f, 1.f, area.getCentreX(), area.getCentreY()));
            g.drawImage (switchArt, area, juce::RectanglePlacement::stretchToFit);
            g.restoreState();
        }
        else
        {
            auto halfFill = [&] (juce::Rectangle<float> r, bool on)
            {
                g.setColour (on ? kPencil : kPaper);
                g.fillRect (r);
                g.setColour (kPencil);
                g.drawRect (r, 1.15f);
            };
            halfFill (left, ! sync);
            halfFill (right, sync);
        }
        const juce::String leftName = button.getProperties().getWithDefault ("pairL", "rate").toString();
        const juce::String rightName = button.getProperties().getWithDefault ("pairR", "sync").toString();
        g.setFont (faceFont (11.f));
        g.setColour (kPencil);
        g.drawText (leftName, left, juce::Justification::centred, false);
        g.drawText (rightName, right, juce::Justification::centred, false);
        return;
    }

    if (switchArt.isValid())
    {
        auto area = button.getLocalBounds().toFloat().reduced (0.5f);
        g.saveState();
        if (button.getToggleState())
            g.addTransform (juce::AffineTransform::scale (-1.f, 1.f, area.getCentreX(), area.getCentreY()));
        g.drawImage (switchArt, area, juce::RectanglePlacement::stretchToFit);
        g.restoreState();
        if (button.getButtonText().isNotEmpty())
        {
            g.setColour (kPencil);
            g.setFont (faceFont (12.f));
            g.drawText (button.getButtonText(), area.reduced (4.f, 0.f),
                        juce::Justification::centred, false);
        }
        return;
    }

    // Switch art flips across its centre. On is the mirror of off.
    auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    auto track = area.removeFromLeft (juce::jmin (area.getWidth(), area.getHeight() * 1.8f));
    const float radius = track.getHeight() * 0.5f;
    g.setColour (kPaper);
    g.fillRoundedRectangle (track, radius);
    g.setColour (kPencil);
    g.drawRoundedRectangle (track.reduced (0.6f), radius, 1.3f);

    const float thumb = track.getHeight() - 5.f;
    const float x = button.getToggleState()
                        ? track.getRight() - thumb - 2.5f
                        : track.getX() + 2.5f;
    g.fillRoundedRectangle (x, track.getY() + 2.5f, thumb, thumb, 2.f);

    if (button.getButtonText().isNotEmpty())
    {
        g.setFont (faceFont (12.f));
        g.drawText (button.getButtonText(), area.reduced (4.f, 0.f),
                    juce::Justification::centredLeft, false);
    }
}

juce::Rectangle<int> ClicksEditor::Look::getTooltipBounds (const juce::String& tip, juce::Point<int> pos,
                                                          juce::Rectangle<int> parent)
{
    const int textW = 260;
    juce::AttributedString words;
    words.setText (tip);
    words.setFont (faceFont (13.f));
    words.setColour (kPencil);
    juce::TextLayout laid;
    laid.createLayout (words, (float) textW);
    const int w = textW + 14;
    const int h = (int) std::ceil (laid.getHeight()) + 10;
    auto box = juce::Rectangle<int> (pos.x + 12, pos.y + 16, w, h);
    if (box.getRight() > parent.getRight())
        box.setX (parent.getRight() - w);
    if (box.getBottom() > parent.getBottom())
        box.setY (pos.y - h - 6);
    return box.constrainedWithin (parent);
}

void ClicksEditor::Look::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    g.setColour (kPaper);
    g.fillRoundedRectangle (0.f, 0.f, (float) width, (float) height, 3.f);
    g.setColour (kPencil);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.f, (float) height - 1.f, 3.f, 1.2f);
    g.setFont (faceFont (13.f));
    g.drawFittedText (text, 7, 4, width - 14, height - 8, juce::Justification::centredLeft, 8, 0.85f);
}

ClicksEditor::ClicksEditor (ClicksProcessor& synth)
    : AudioProcessorEditor (&synth),
      clicks (synth),
      tips (this, 680),
      keyboard (synth.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    look.face = juce::Typeface::createSystemTypefaceFor (BinaryData::Chicago12_otf,
                                                          BinaryData::Chicago12_otfSize);
    field = juce::ImageFileFormat::loadFrom (BinaryData::starfield_png, BinaryData::starfield_pngSize);
    mark = juce::ImageFileFormat::loadFrom (BinaryData::nullatomo_png, BinaryData::nullatomo_pngSize);
    bot = juce::ImageFileFormat::loadFrom (BinaryData::samrobot_jpg, BinaryData::samrobot_jpgSize);

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
    tips.setLookAndFeel (&look);

    auto addDrop = [this] (juce::ComboBox& box, juce::Label& caption, const juce::String& id,
                           const juce::String& name, const juce::String& tip)
        -> std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (clicks.apvts.getParameter (id)))
            for (int i = 0; i < choice->choices.size(); ++i)
                box.addItem (choice->choices[i], i + 1);
        box.setLookAndFeel (&look);
        box.setTooltip (tip);
        box.setColour (juce::ComboBox::backgroundColourId, kPaper);
        box.setColour (juce::ComboBox::textColourId, kPencil);
        box.setColour (juce::ComboBox::outlineColourId, kPencil);
        box.setColour (juce::ComboBox::arrowColourId, kPencil);
        box.getProperties().set ("paramId", id);
        box.addMouseListener (this, true);
        addAndMakeVisible (box);
        caption.setText (name, juce::dontSendNotification);
        caption.setFont (chicagoFont (15.f));
        caption.setColour (juce::Label::textColourId, kPencil);
        caption.setJustificationType (juce::Justification::centredLeft);
        caption.setLookAndFeel (&look);
        caption.setTooltip (tip);
        caption.addMouseListener (this, true);
        addAndMakeVisible (caption);
        return std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (clicks.apvts, id, box);
    };
    waveAttachment = addDrop (waveBox, waveLabel, "wave", "wave",
                              "Wave: Sine, saw up, saw down, square, and triangle follow Width. Noise is a hiss. Sam is the locked 1982 speech chip. eSpeak is a second speech voice with its own knobs. Either one lights the robot mouth.");
    polyAttachment = addDrop (polyBox, polyLabel, "poly", "voices",
                              "Voices: Mono keeps the newest key. Poly lets notes overlap.");
    scatAttachment = addDrop (scatBox, scatLabel, "scatmode", "drift",
                              "Drift: Warble sways the whole mix passing through the tape, sub and echoes included. Wow at the rate knob, a lighter flutter on top. Repeat walks each source through a fixed run of steps. The rate knob sets how fast either one moves, apart from tape speed.");
    subAttachment = addDrop (subBox, subLabel, "submode", "mode",
                             "Sub: Add and subtract mix it in. Multiply is ring modulation. Divide is the tone over the sub. AM is loudness. FM bends the pitch. Min and max pick the lower or higher sample.");
    subWaveAttachment = addDrop (subWaveBox, subWaveLabel, "subwave", "shape",
                                 "Shape: sine, square, saw up, saw down, or triangle. The sub has its own width.");
    filtTypeAttachment = addDrop (filtTypeBox, filtTypeLabel, "filttype", "type",
                                  "Type: Off leaves the path dry. Lowpass, highpass, bandpass, and Remove. Remove takes out one frequency: Cut is that frequency, and Reso up makes the hole narrower. Formant and resonator follow.");
    filt2TypeAttachment = addDrop (filt2TypeBox, filt2TypeLabel, "filt2type", "type",
                                   "Filter 2 type. Off leaves this one dry. Remove takes out one frequency, Cut picks it, Reso up makes the hole narrower. It runs just after filter 1, at the same chain number.");
    fxOrderAttachment = addDrop (fxOrderBox, fxOrderLabel, "fxorder", "order",
                                 "Order: crush, filter, delay, tape, and the compressor. The chain numbers on each effect choose the path. Both filters run at the filter number, one then two.");
    fxOrderBox.setVisible (false);
    fxOrderLabel.setVisible (false);
    const char* slotIds[6] = { "tapeslot", "crushslot", "filtslot", "delayslot", "revslot", "compslot" };
    const char* slotTips[6] = {
        "Tape place in the chain. 1 runs first. Everything before it in the chain gets the warble and the deck colour. Picking a number another effect already has swaps the two.",
        "Crush place in the chain. 1 runs first. Picking a number another effect already has swaps the two.",
        "Filter place in the chain. 1 runs first. Both filters run here, filter 1 then filter 2. Picking a number another effect already has swaps the two.",
        "Delay place in the chain. 1 runs first. Picking a number another effect already has swaps the two.",
        "Reverb place in the chain. 1 runs first. Picking a number another effect already has swaps the two.",
        "Compress place in the chain. 1 runs first. The limiter stays last. Picking a number another effect already has swaps the two."
    };
    for (int i = 0; i < 6; ++i)
    {
        auto& box = slotBox[i];
        for (int n = 1; n <= 6; ++n)
            box.addItem (juce::String (n), n);
        box.setLookAndFeel (&look);
        box.setColour (juce::ComboBox::backgroundColourId, kPaper);
        box.setColour (juce::ComboBox::textColourId, kPencil);
        box.setColour (juce::ComboBox::outlineColourId, kPencil);
        box.setColour (juce::ComboBox::arrowColourId, kPencil);
        box.setTooltip (slotTips[i]);
        box.getProperties().set ("paramId", slotIds[i]);
        box.addMouseListener (this, true);
        addAndMakeVisible (box);
        slotAttachment[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            clicks.apvts, slotIds[i], box);
    }
    for (int i = 0; i < 6; ++i)
        slotBox[i].onChange = [this, i] { routeSlot (i); };
    for (int i = 0; i < 4; ++i)
    {
        const auto n = juce::String (i + 1);
        lfoShapeAttachment[i] = addDrop (lfoShapeBox[i], lfoShapeLabel[i], "lfo" + n + "shape", "shape",
                                         "LFO " + n + " shape: sine, triangle, saw up, saw down, square, or sample-hold.");
        lfoDestAttachment[i] = addDrop (lfoDestBox[i], lfoDestLabel[i], "lfo" + n + "dest", "dest",
                                        "LFO " + n + " destination. The chosen control wears this colour.");
    }

    // name, caption under the knob, and the line shown in the title bar while the pointer is on it.
    // Crush is bits + hold. Glitch is the holes, and it also lengthens hold.
    struct Spec { const char* id; const char* name; const char* hint; const char* tip; };
    // Rows follow the sound: sub and playback, then the crusher, then the click.
    for (auto spec : { Spec { "subtune", "tune", "sub semis",
                              "Tune: sub pitch in semitones from the key. -12 is one octave down. It follows every note." },
                       Spec { "submix", "mix", "sub amount",
                              "Mix: how hard the sub joins. Zero leaves every mode dry." },
                       Spec { "slow", "speed", "rate",
                              "Speed: the tape rate for every source, main, sub, and speech. Tape drops the pitch as it slows and roughens the whole mix, without getting louder. Fix keeps the speech pitch and only changes the time. One plays the note as held. Tempo on, this is the beat division. Speech fills that division. Warble speed is the rate knob beside amount." },
                       Spec { "bits", "bits", "crush steps",
                              "Bits: volume steps. 16 is clean. 8 hisses. 2 is a few coarse levels. Set wave to Sine, glitch to 0, and hold to 1." },
                       Spec { "hold", "hold", "crush freeze",
                              "Hold: how many samples a crushed step stays frozen. Longer makes wider stairs on the scope. Set glitch to 0 first." },
                       Spec { "glitch", "glitch", "holes",
                              "Glitch: random holes and a stuck sample. It also lengthens hold, which covers bits and hold." },
                       Spec { "crushmix", "wet", "crush mix",
                              "Wet: how much bits, hold, and glitch replace the clean note. Zero leaves the note dry." },
                       Spec { "seed", "seed", "which pattern",
                              "Seed: which scatter pattern, on every wave. The same key and seed repeat. Warble keeps drifting. Repeat plays one fixed run of that seed." },
                       Spec { "scatter", "amount", "warble",
                              "Amount: how far the tape sways everything passing through it, sub, echoes, Sam and eSpeak included. A quarter is a gentle wobble, half is about a semitone, full is a drunk deck. Repeat mode uses this as its pitch spread." },
                       Spec { "steps", "steps", "in the loop",
                              "Steps: how many pitches Repeat plays, then loops. The rate knob sets how long that run takes. Tape speed does not." },
                       Spec { "width", "width", "pulse",
                              "Width: where the main wave flips. Half is the plain shape. Square becomes a pulse. Sine, saw up, saw down, and triangle lean the same way. Noise and speech stay plain. The sub has its own width." },
                       Spec { "subfine", "fine", "cents",
                              "Fine: cents on top of the sub tune. Zero leaves the semitone where tune set it." },
                       Spec { "subwidth", "width", "pulse",
                              "Width: where the sub wave flips. Half is the plain shape." },
                       Spec { "scatglide", "glide", "warble slide",
                              "Glide: in Warble, adds a slow random wander on top of the wow, and sets how long each wander takes to reach its next pitch. Zero is pure wow and flutter. In Repeat, how long each step takes to slide to the next pitch." },
                       Spec { "srcglide", "slide", "source glide",
                              "Slide: how long the main source glides between keys. Zero jumps on the new note. The sub ignores this." } })
        generator.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));
    wobble.push_back (addKnob ("wobrate", "rate", "warble",
                               "Warble rate: the wow speed in Hz, with a lighter flutter riding about five times faster. Under 2 Hz is a slow sway, 4 to 8 is a wobbly deck, 30 is a shiver. Repeat walks its steps at this rate. Tape speed does not move this."));

    for (auto spec : {                        Spec { "samspeed", "speed", "sam talk",
                              "Speed: how fast Sam talks. It stays the same on every key, and a held note follows the knob. 72 is the 1982 voice. Higher is slower." },
                       Spec { "sampitch", "pitch", "sam pitch",
                              "Pitch: Sam's own pitch. 64 is the original. A held note follows this knob. Keys change the pitch and leave the talk speed alone." },
                       Spec { "sammouth", "mouth", "sam mouth",
                              "Mouth: Sam's mouth shape. 128 is the original. A held note follows the knob." },
                       Spec { "samthroat", "throat", "sam throat",
                              "Throat: Sam's throat shape. 128 is the original. A held note follows the knob." },
                       Spec { "samslow", "slow", "near stop",
                              "Slow: stretches the locked word on the fly, down to a near stop. One keeps the pace Speed and the Sam speed knob already set. A held note follows the knob. It does not re-render Sam." } })
        sam.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));

    for (auto spec : { Spec { "espeakrate", "rate", "words a minute",
                              "Rate: how fast eSpeak talks, in words a minute. 175 is the usual pace. Higher is faster. A held note follows the knob." },
                       Spec { "espeakpitch", "pitch", "espeak pitch",
                              "Pitch: eSpeak's own pitch, 0 to 99. 50 is the usual voice. Keys change the pitch and leave the talk speed alone." },
                       Spec { "espeakrange", "range", "intonation",
                              "Range: how far the pitch moves inside the sentence. 50 is the usual rise and fall. Zero is flatter." },
                       Spec { "espeakgap", "gap", "word pause",
                              "Gap: extra pause between words. Each step is about a hundredth of a second. Zero keeps the words close." },
                       Spec { "espeakvoice", "voice", "variant",
                              "Voice: en, male and female variants, whisper, croak, and Klatt. The row swaps to these knobs while eSpeak is selected." } })
        speak.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));
    if (speak.size() > 4 && speak[4] != nullptr)
    {
        static const char* voiceNames[] = {
            "en", "m1", "m3", "m5", "f1", "f3", "f5", "whisper", "croak", "klatt", "klatt2", "klatt3"
        };
        speak[4]->slider.textFromValueFunction = [] (double value)
        {
            const int i = juce::jlimit (0, 11, (int) std::lround (value));
            return juce::String (voiceNames[i]);
        };
        speak[4]->slider.valueFromTextFunction = [] (const juce::String& text)
        {
            for (int i = 0; i < 12; ++i)
                if (text.equalsIgnoreCase (voiceNames[i]))
                    return (double) i;
            return text.getDoubleValue();
        };
        speak[4]->slider.updateText();
    }

    for (auto spec : { Spec { "attack", "A", "",
                              "Attack: fade-in time after the key goes down. Sync steps this through beat divisions." },
                       Spec { "decay", "D", "",
                              "Decay: time to fall from full level down to sustain. Sync steps this through beat divisions." },
                       Spec { "sustain", "S", "",
                              "Sustain: level while the key stays down." },
                       Spec { "release", "R", "",
                              "Release: fade time after the key comes up. Sync steps this through beat divisions." } })
        env.push_back (addFader (spec.id, spec.name, spec.hint, spec.tip));

    for (auto spec : { Spec { "subattack", "A", "",
                              "Sub attack: fade-in of the sub after the key goes down. Sync steps this through beat divisions." },
                       Spec { "subdecay", "D", "",
                              "Sub decay: time for the sub to fall from full level down to sustain. Sync steps this through beat divisions." },
                       Spec { "subsustain", "S", "",
                              "Sub sustain: sub level while the key stays down." },
                       Spec { "subrelease", "R", "",
                              "Sub release: fade of the sub after the key comes up. Sync steps this through beat divisions." } })
        senv.push_back (addFader (spec.id, spec.name, spec.hint, spec.tip));

    for (auto spec : { Spec { "fattack", "A", "",
                              "Filter attack: how fast the filter envelope opens. Sync steps this through beat divisions." },
                       Spec { "fdecay", "D", "",
                              "Filter decay: fall from the top of the filter envelope down to its sustain. Sync steps this through beat divisions." },
                       Spec { "fsustain", "S", "",
                              "Filter sustain: envelope level while the key is down." },
                       Spec { "frelease", "R", "",
                              "Filter release: how the filter envelope closes after the key comes up. Sync steps this through beat divisions." },
                       Spec { "fenv", "amt", "",
                              "Filter envelope amount. Positive opens the cutoff as the envelope rises. Negative closes it. Zero leaves the cutoff still." } })
        fenv.push_back (addFader (spec.id, spec.name, spec.hint, spec.tip));

    for (auto spec : { Spec { "f2attack", "A", "",
                              "Filter 2 attack: how fast this filter's envelope opens. Sync steps this through beat divisions." },
                       Spec { "f2decay", "D", "",
                              "Filter 2 decay: fall from the top of this envelope down to its sustain. Sync steps this through beat divisions." },
                       Spec { "f2sustain", "S", "",
                              "Filter 2 sustain: envelope level while the key is down." },
                       Spec { "f2release", "R", "",
                              "Filter 2 release: how this envelope closes after the key comes up. Sync steps this through beat divisions." },
                       Spec { "f2env", "amt", "",
                              "Filter 2 envelope amount. Positive opens this cutoff as the envelope rises. Negative closes it. Zero leaves the cutoff still." } })
        fenv2.push_back (addFader (spec.id, spec.name, spec.hint, spec.tip));

    for (auto spec : { Spec { "delay", "delay", "echo time",
                              "Delay: time before the echo returns. Sync steps this through beat divisions, triplets and dotted notes included, from a 32nd-note triplet up to 8 bars." },
                       Spec { "feedback", "feedback", "repeats",
                              "Feedback: how much echo is fed back. Higher rings longer." },
                       Spec { "echo", "echo", "dry / wet",
                              "Echo: balance from the dry note to the echo." },
                       Spec { "level", "level", "loudness",
                              "Level: output loudness, in decibels." } })
        output.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));

    // Cut and reso sit in the filters card. They grey out while type is Off.
    for (auto spec : {                        Spec { "filtcut", "cut", "hertz",
                              "Cut: filter frequency. On Remove this is the frequency that is taken out. On a formant this walks the vowel. On a resonator this is the ring." },
                       Spec { "filtq", "reso", "sharpness",
                              "Reso: how hard the filter rings at the cutoff. On Remove, higher makes a thinner hole around Cut. On a formant this is the vowel bandwidth." },
                       Spec { "formshift", "shift", "vowel size",
                              "Shift: scales every formant. Lower is a bigger mouth. Higher is smaller." },
                       Spec { "formspread", "spread", "vowel gap",
                              "Spread: pulls the upper formants away from the first. One keeps the written vowel." },
                       Spec { "formbreath", "breath", "vowel air",
                              "Breath: noise through the vowel. Zero is a clean formant." },
                       Spec { "resodamp", "damp", "ring dull",
                              "Damp: dulls the resonator. Zero stays bright." },
                       Spec { "resotune", "ring", "semis",
                              "Ring: moves the resonator in semitones from the cutoff." },
                       Spec { "resostack", "stack", "octave ring",
                              "Stack: adds a second ring an octave up. Zero is one pitch." } })
        filter.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));

    for (auto spec : {                        Spec { "filt2cut", "cut", "hertz",
                              "Filter 2 cut: frequency of the second filter. On Remove this is the frequency that is taken out. On a formant this walks the vowel. On a resonator this is the ring." },
                       Spec { "filt2q", "reso", "sharpness",
                              "Filter 2 reso: how hard the second filter rings at its cutoff. On Remove, higher makes a thinner hole around Cut." },
                       Spec { "filt2mix", "mix", "dry / wet",
                              "Filter 2 mix: blend from the dry note to this filter. Full is the whole filter. Zero skips it. Filter 1's blend is the filt fader." },
                       Spec { "form2shift", "shift", "vowel size",
                              "Filter 2 shift: scales every formant. Lower is a bigger mouth. Higher is smaller." },
                       Spec { "form2spread", "spread", "vowel gap",
                              "Filter 2 spread: pulls the upper formants away from the first. One keeps the written vowel." },
                       Spec { "form2breath", "breath", "vowel air",
                              "Filter 2 breath: noise through the vowel. Zero is a clean formant." },
                       Spec { "reso2damp", "damp", "ring dull",
                              "Filter 2 damp: dulls the resonator. Zero stays bright." },
                       Spec { "reso2tune", "ring", "semis",
                              "Filter 2 ring: moves the resonator in semitones from the cutoff." },
                       Spec { "reso2stack", "stack", "octave ring",
                              "Filter 2 stack: adds a second ring an octave up. Zero is one pitch." } })
        filter2.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));

    for (int i = 1; i <= 4; ++i)
    {
        const auto n = juce::String (i);
        lfo.push_back (addKnob ("lfo" + n + "rate", "rate", "hz",
                                "LFO " + n + " rate. Rate is hertz. Sync steps through beat divisions. Pick one."));
        lfo.push_back (addKnob ("lfo" + n + "depth", "depth", "amount",
                                "LFO " + n + " depth. Zero leaves the chosen knob still."));
    }
    for (auto spec : { Spec { "compthresh", "thresh", "db",
                              "Threshold: peaks above this get squeezed. Zero dB only catches full scale." },
                       Spec { "compratio", "ratio", "squeeze",
                              "Ratio: how hard those peaks are turned down. One leaves the sound alone." },
                       Spec { "compgain", "makeup", "db",
                              "Makeup: gain after the squeeze. Use it when a formant or filter drops the level. Zero is unity." },
                       Spec { "limit", "limit", "ceiling",
                              "Limit: ceiling after the compressor and the output level. Init is -1 dB, just under full scale, so a fresh patch does not clip. Peaks that cross it are flattened. 0 dB is as loud as the output will go." },
                       Spec { "limitdist", "dist", "bend",
                              "Dist: how hard the note is pushed into the limiter. Zero leaves everything under the ceiling clean. Full bends the whole sound so the clip is easy to hear. The logo trace is this output." } })
        comp.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));
    for (auto spec : { Spec { "revsize", "size", "room",
                              "Size: how long the reverb rings. Small is a tight box. Full is a long room." },
                       Spec { "revdamp", "damp", "dull",
                              "Damp: takes the brightness out of the repeats. Zero stays bright." },
                       Spec { "revmix", "mix", "wet",
                              "Mix: how much of the room is in the sound. Zero is dry." } })
        reverb.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));
    for (auto spec : { Spec { "srclevel", "src", "source",
                              "Source: how much of the main wave or Sam reaches the output. Full leaves it as played." },
                       Spec { "sublevel", "sub", "sub",
                              "Sub: how much of the sub oscillator reaches the output. Full leaves the sub mode as set." },
                       Spec { "crushlevel", "crush", "crush",
                              "Crush: how much of the bit crush is in the output. Full uses the wet knob as set. Zero leaves the note clean." },
                       Spec { "filtmix", "filt", "filter",
                              "Filter 1: blend from the dry note to the first filter. Full is the whole filter. Zero skips it. Filter 2 has its own mix on its tab." },
                       Spec { "delaylevel", "delay", "delay",
                              "Delay: how much echo is in the output. Full uses the echo knob as set. Zero leaves the repeats out." },
                       Spec { "compmix", "comp", "compress",
                              "Compress: blend from the dry mix to the squeezed one. Full is the whole compressor. Zero skips it." } })
        mix.push_back (addKnob (spec.id, spec.name, spec.hint, spec.tip));

    auto styleTitle = [this] (juce::Label& label, const juce::String& text, juce::Colour colour)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (chicagoFont (16.f));
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setLookAndFeel (&look);
        addAndMakeVisible (label);
    };
    for (int i = 0; i < 4; ++i)
    {
        styleTitle (lfoTitle[i], "lfo " + juce::String (i + 1), kLfoCol[i]);
        lfoTitle[i].setVisible (false);
        auto& tab = lfoTab[i];
        tab.setButtonText ("lfo " + juce::String (i + 1));
        tab.setLookAndFeel (&look);
        tab.setTooltip ("Show LFO " + juce::String (i + 1) + ". All four keep running.");
        tab.onClick = [this, i]
        {
            lfoPage = i;
            resized();
        };
        addAndMakeVisible (tab);
    }

    keyboard.setLookAndFeel (&look);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, kPaper);
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff2c261f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, kPencil);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x33221c16));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0x55221c16));
    keyboard.setAvailableRange (36, 84);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setVisible (false);

    auto styleButton = [this] (juce::TextButton& button, const juce::String& tip)
    {
        button.setLookAndFeel (&look);
        button.setColour (juce::TextButton::buttonColourId, kPaper);
        button.setColour (juce::TextButton::textColourOffId, kPencil);
        button.setTooltip (tip);
        button.addMouseListener (this, true);
        addAndMakeVisible (button);
    };
    for (int i = 0; i < 2; ++i)
    {
        auto& tab = filtTab[i];
        tab.setButtonText (juce::String (i + 1));
        tab.setClickingTogglesState (false);
        tab.setColour (juce::TextButton::buttonOnColourId, kPencil);
        tab.setColour (juce::TextButton::textColourOnId, kPaper);
        styleButton (tab, "Filter " + juce::String (i + 1) + ". Both keep running. 1 is first, then 2.");
        tab.onClick = [this, i]
        {
            filtPage = i;
            resized();
            refreshFilter();
        };
    }
    initButton.setButtonText ("init");
    saveButton.setButtonText ("save");
    loadButton.setButtonText ("load");
    prevButton.setButtonText ("<");
    nextButton.setButtonText (">");
    styleButton (initButton, "Init: sine, poly, sub sine at full mix, source envelopes on 1/8, sub attack on 1 bar, output and effect places as the opening patch.");
    styleButton (saveButton, "Save: write the knobs and the Sam word to a preset.");
    styleButton (loadButton, "Load: read a preset back in.");
    styleButton (prevButton, "Previous shipped preset. Click the name for the list.");
    styleButton (nextButton, "Next shipped preset. Click the name for the list.");
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    styleButton (lockButton, "Lock: render the word now. Return in the phrase box does the same. Speed, pitch, mouth, and throat also follow a note you are already holding. A line that does not fit is refused.");
    lockButton.setButtonText ("lock");
    styleButton (darkButton, "Dark: invert every colour on the panel. The choice is kept between sessions.");
    darkButton.onClick = [this]
    {
        dark = ! dark;
        if (prefs != nullptr)
        {
            prefs->setValue ("dark", dark);
            prefs->saveIfNeeded();
        }
        applyTheme();
    };
    {
        juce::PropertiesFile::Options opt;
        opt.applicationName = "NULL GLITCH";
        opt.filenameSuffix = "settings";
        opt.folderName = "Logic Beach";
        opt.osxLibrarySubFolder = "Application Support";
        opt.storageFormat = juce::PropertiesFile::storeAsXML;
        prefs = std::make_unique<juce::PropertiesFile> (opt);
        dark = prefs->getBoolValue ("dark", false);
    }
    initButton.onClick = [this] { initPreset(); };
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };
    presetName = clicks.apvts.state.getProperty ("preset").toString();
    presetBox.setFont (chicagoFont (15.f));
    presetBox.setColour (juce::Label::textColourId, kPencil);
    presetBox.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.setBorderSize (juce::BorderSize<int> (0, 6, 0, 4));
    presetBox.setMinimumHorizontalScale (0.6f);
    presetBox.setLookAndFeel (&look);
    presetBox.setTooltip ("Preset: click for the shipped list. The arrows step through it.");
    presetBox.setInterceptsMouseClicks (true, false);
    presetBox.addMouseListener (this, false);
    addAndMakeVisible (presetBox);
    showPreset();
    lockButton.onClick = [this] { showNote (clicks.lockSpeech()); };

    sayLabel.setText ("say", juce::dontSendNotification);
    sayLabel.setFont (chicagoFont (15.f));
    sayLabel.setColour (juce::Label::textColourId, kPencil);
    sayLabel.setJustificationType (juce::Justification::centredLeft);
    sayLabel.setLookAndFeel (&look);
    sayLabel.setTooltip ("Say: the line to lock. Sam is Software Automatic Mouth, the 1982 speech program from Don't Ask Software. eSpeak NG is the other speech voice.");
    sayLabel.addMouseListener (this, true);
    addAndMakeVisible (sayLabel);

    sayBox.setMultiLine (false);
    sayBox.setReturnKeyStartsNewLine (false);
    sayBox.setFont (chicagoFont (15.f));
    sayBox.setIndents (6, 4);
    sayBox.setInputRestrictions (40);
    sayBox.setText ("hello", juce::dontSendNotification);
    sayBox.setColour (juce::TextEditor::backgroundColourId, kPaper);
    sayBox.setColour (juce::TextEditor::textColourId, kPencil);
    sayBox.setColour (juce::TextEditor::outlineColourId, kPencil);
    sayBox.setColour (juce::TextEditor::focusedOutlineColourId, kPencil);
    sayBox.setTooltip ("Say: the line the speech voice speaks. Digits are read as numbers. Press return to lock it. A held note follows the line. A line that will not fit is refused, and the previous line stays.");
    sayBox.addMouseListener (this, true);
    sayBox.onTextChange = [this]
    {
        clicks.setSamText (sayBox.getText());
    };
    sayBox.onReturnKey = [this]
    {
        clicks.setSamText (sayBox.getText());
        showNote (clicks.lockSpeech());
    };
    addAndMakeVisible (sayBox);

    syncTick.setLookAndFeel (&look);
    syncTick.setClickingTogglesState (true);
    syncTick.setTooltip ("Tempo: Speed steps from a 32nd note to 8 bars. Speech fills that division and restarts on the beat. Repeat loops inside it. Off, Speed is a free playback rate.");
    syncTick.addMouseListener (this, true);
    addAndMakeVisible (syncTick);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        clicks.apvts, "slowsync", syncTick);
    slowRetrig.setButtonText ("trig");
    slowRetrig.setLookAndFeel (&look);
    slowRetrig.setClickingTogglesState (true);
    slowRetrig.setTooltip ("Retrig: the beat division starts on the note. Speech restarts with the key, and Repeat begins again at its first step. Off, both follow the host clock.");
    slowRetrig.addMouseListener (this, true);
    addAndMakeVisible (slowRetrig);
    slowRetrigAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        clicks.apvts, "slowretrig", slowRetrig);

    auto addTick = [this] (juce::ToggleButton& button, const char* id, const juce::String& tip)
    {
        button.setLookAndFeel (&look);
        button.setClickingTogglesState (true);
        button.setTooltip (tip);
        button.addMouseListener (this, true);
        addAndMakeVisible (button);
        return std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (clicks.apvts, id, button);
    };
    delaySyncAttachment = addTick (delaySync, "delaysync", "Sync: delay time steps through beat divisions. Off, the knob is seconds.");
    delayRetrig.setButtonText ("trig");
    delayRetrigAttachment = addTick (delayRetrig, "delayretrig", "Retrig: a new note clears the echoes, so the repeats start with that note. Off, the tail keeps ringing.");
    attackSyncAttachment = addTick (attackSync, "attacksync", "Sync: attack time steps through beat divisions. Off, the knob is seconds.");
    decaySyncAttachment = addTick (decaySync, "decaysync", "Sync: decay time steps through beat divisions. Off, the knob is seconds.");
    releaseSyncAttachment = addTick (releaseSync, "releasesync", "Sync: release time steps through beat divisions. Off, the knob is seconds.");
    for (int i = 0; i < 4; ++i)
    {
        const juce::String id = "lfo" + juce::String (i + 1) + "sync";
        lfoSyncAttachment[i] = addTick (lfoSync[i], id.toRawUTF8(),
                                        "LFO " + juce::String (i + 1) + ": rate is hertz, sync is a beat division. One of the two is active.");
        lfoRetrig[i].setButtonText ("trig");
        lfoRetrigAttachment[i] = addTick (lfoRetrig[i], ("lfo" + juce::String (i + 1) + "retrig").toRawUTF8(),
                                          "LFO " + juce::String (i + 1) + " retrig: a new note starts this LFO again at the top of its shape. Off, it keeps running. It works in hertz and in sync.");
    }
    fAttackSyncAttachment = addTick (fAttackSync, "fattacksync", "Sync: filter attack steps through beat divisions.");
    fDecaySyncAttachment = addTick (fDecaySync, "fdecaysync", "Sync: filter decay steps through beat divisions.");
    fReleaseSyncAttachment = addTick (fReleaseSync, "freleasesync", "Sync: filter release steps through beat divisions.");
    f2AttackSyncAttachment = addTick (f2AttackSync, "f2attacksync", "Sync: filter 2 attack steps through beat divisions.");
    f2DecaySyncAttachment = addTick (f2DecaySync, "f2decaysync", "Sync: filter 2 decay steps through beat divisions.");
    f2ReleaseSyncAttachment = addTick (f2ReleaseSync, "f2releasesync", "Sync: filter 2 release steps through beat divisions.");
    subAttackSyncAttachment = addTick (subAttackSync, "subattacksync", "Sync: sub attack steps through beat divisions. Off, the fader is seconds.");
    subDecaySyncAttachment = addTick (subDecaySync, "subdecaysync", "Sync: sub decay steps through beat divisions. Off, the fader is seconds.");
    subReleaseSyncAttachment = addTick (subReleaseSync, "subreleasesync", "Sync: sub release steps through beat divisions. Off, the fader is seconds.");

    auto armPair = [] (juce::ToggleButton& button)
    {
        button.getProperties().set ("pair", 1);
        button.setClickingTogglesState (false);
        button.onClick = [&button]
        {
            const bool sync = button.getMouseXYRelative().x >= button.getWidth() / 2;
            button.setToggleState (sync, juce::sendNotificationSync);
        };
    };
    armPair (syncTick);
    pitchTick.setLookAndFeel (&look);
    pitchTick.setClickingTogglesState (true);
    pitchTick.setTooltip ("Tape lets Speed drop the pitch and roughen the whole mix, the way a deck slows down. Fix keeps the speech pitch and only changes the time. Warble sways the mix either way.");
    pitchTick.addMouseListener (this, true);
    addAndMakeVisible (pitchTick);
    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        clicks.apvts, "pitchfix", pitchTick);
    pitchTick.getProperties().set ("pairL", "tape");
    pitchTick.getProperties().set ("pairR", "fix");
    armPair (pitchTick);
    armPair (delaySync);
    armPair (attackSync);
    armPair (decaySync);
    armPair (releaseSync);
    armPair (fAttackSync);
    armPair (fDecaySync);
    armPair (fReleaseSync);
    armPair (f2AttackSync);
    armPair (f2DecaySync);
    armPair (f2ReleaseSync);
    armPair (subAttackSync);
    armPair (subDecaySync);
    armPair (subReleaseSync);
    for (int i = 0; i < 4; ++i)
        armPair (lfoSync[i]);

    outMeter.setTooltip ("Output: how loud the sound is after the limiter. The mark is the ceiling.");
    outMeter.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    outMeter.setInterceptsMouseClicks (true, false);
    addAndMakeVisible (outMeter);

    for (auto* child : getChildren())
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (child))
            slider->onValueChange = [this, slider]
            {
                repaint (slider->getBoundsInParent().expanded (8));
            };
        else if (auto* button = dynamic_cast<juce::Button*> (child))
            button->onStateChange = [this, button]
            {
                repaint (button->getBoundsInParent().expanded (8));
            };
    }

    setResizable (false, false);
    refreshSam();
    refreshFilter();
    refreshSource();
    refreshCrush();
    refreshLfo();
    refreshScatter();
    setSize (1216, 1082);
    applyTheme();
    startTimerHz (30);
}

ClicksEditor::~ClicksEditor()
{
    stopTimer();
    for (auto* child : getChildren())
        child->setComponentEffect (nullptr);
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
    darkButton.setLookAndFeel (nullptr);
    initButton.setLookAndFeel (nullptr);
    saveButton.setLookAndFeel (nullptr);
    loadButton.setLookAndFeel (nullptr);
    prevButton.setLookAndFeel (nullptr);
    nextButton.setLookAndFeel (nullptr);
    presetBox.setLookAndFeel (nullptr);
    lockButton.setLookAndFeel (nullptr);
    syncTick.setLookAndFeel (nullptr);
    slowRetrig.setLookAndFeel (nullptr);
    delayRetrig.setLookAndFeel (nullptr);
    pitchTick.setLookAndFeel (nullptr);
    delaySync.setLookAndFeel (nullptr);
    attackSync.setLookAndFeel (nullptr);
    decaySync.setLookAndFeel (nullptr);
    releaseSync.setLookAndFeel (nullptr);
    for (int i = 0; i < 4; ++i)
    {
        lfoSync[i].setLookAndFeel (nullptr);
        lfoRetrig[i].setLookAndFeel (nullptr);
        lfoTitle[i].setLookAndFeel (nullptr);
        lfoTab[i].setLookAndFeel (nullptr);
        lfoShapeBox[i].setLookAndFeel (nullptr);
        lfoDestBox[i].setLookAndFeel (nullptr);
        lfoShapeLabel[i].setLookAndFeel (nullptr);
        lfoDestLabel[i].setLookAndFeel (nullptr);
    }
    waveBox.setLookAndFeel (nullptr);
    polyBox.setLookAndFeel (nullptr);
    scatBox.setLookAndFeel (nullptr);
    subBox.setLookAndFeel (nullptr);
    subWaveBox.setLookAndFeel (nullptr);
    filtTypeBox.setLookAndFeel (nullptr);
    filt2TypeBox.setLookAndFeel (nullptr);
    filt2TypeLabel.setLookAndFeel (nullptr);
    for (int i = 0; i < 2; ++i)
        filtTab[i].setLookAndFeel (nullptr);
    fxOrderBox.setLookAndFeel (nullptr);
    fxOrderLabel.setLookAndFeel (nullptr);
    for (int i = 0; i < 6; ++i)
        slotBox[i].setLookAndFeel (nullptr);
    lfoShapeBox[0].setLookAndFeel (nullptr);
    lfoShapeBox[1].setLookAndFeel (nullptr);
    fAttackSync.setLookAndFeel (nullptr);
    fDecaySync.setLookAndFeel (nullptr);
    fReleaseSync.setLookAndFeel (nullptr);
    f2AttackSync.setLookAndFeel (nullptr);
    f2DecaySync.setLookAndFeel (nullptr);
    f2ReleaseSync.setLookAndFeel (nullptr);
    subAttackSync.setLookAndFeel (nullptr);
    subDecaySync.setLookAndFeel (nullptr);
    subReleaseSync.setLookAndFeel (nullptr);
    lfoDestBox[0].setLookAndFeel (nullptr);
    lfoDestBox[1].setLookAndFeel (nullptr);
    lfoDestLabel[0].setLookAndFeel (nullptr);
    lfoDestLabel[1].setLookAndFeel (nullptr);
    sayLabel.setLookAndFeel (nullptr);
    keyboard.setLookAndFeel (nullptr);
    for (auto* group : { &generator, &sam, &speak, &filter, &filter2, &output, &env, &senv, &fenv, &fenv2, &comp, &reverb, &wobble, &mix, &lfo })
        for (auto& knob : *group)
            if (knob != nullptr)
            {
                knob->slider.setLookAndFeel (nullptr);
                knob->label.setLookAndFeel (nullptr);
                knob->hint.setLookAndFeel (nullptr);
            }
}

juce::Font ClicksEditor::chicagoFont (float height) const
{
    return look.faceFont (height);
}

void ClicksEditor::tryMidiLearnFrom (juce::Component* target, const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu() && ! event.mods.isRightButtonDown())
        return;
    while (target != nullptr)
    {
        const auto paramId = target->getProperties()["paramId"].toString();
        if (paramId.isNotEmpty())
        {
            if (event.mods.isShiftDown())
            {
                clicks.clearMidiMappingForParam (paramId);
                showNote ("MIDI mapping cleared for " + clicks.paramNameForId (paramId) + ".");
            }
            else
            {
                clicks.armMidiLearn (paramId);
                learnUntil = juce::Time::getMillisecondCounter() + 15000;
                const int cc = clicks.midiCcForParam (paramId);
                if (cc >= 0)
                    showNote ("MIDI learn: move CC " + juce::String (cc + 1) + " or twist any control to remap "
                              + clicks.paramNameForId (paramId) + ".");
                else
                    showNote ("MIDI learn: move a MIDI control for "
                              + clicks.paramNameForId (paramId) + ". Shift-right-click clears.");
            }
            return;
        }
        target = target->getParentComponent();
    }
}

void ClicksEditor::mouseDown (const juce::MouseEvent& event)
{
    if (event.eventComponent == &presetBox)
    {
        showPresetMenu();
        return;
    }
    tryMidiLearnFrom (event.eventComponent, event);
}

void ClicksEditor::choosePreset (int index)
{
    const int count = clicks.getNumPrograms();
    if (count <= 0)
        return;
    index = ((index % count) + count) % count;
    clicks.applyFactoryPreset (index);
    presetName = clicks.apvts.state.getProperty ("preset").toString();
    sayBox.setText (clicks.samText(), juce::dontSendNotification);
    clicks.lockSpeech (false);
    status.clear();
    showPreset();
}

void ClicksEditor::stepPreset (int delta)
{
    choosePreset (clicks.getCurrentProgram() + delta);
}

void ClicksEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&look);
    const int current = clicks.getCurrentProgram();
    for (int i = 0; i < clicks.getNumPrograms(); ++i)
        menu.addItem (i + 1, clicks.getProgramName (i), true, i == current);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBox),
                        [safe = juce::Component::SafePointer<ClicksEditor> (this)] (int picked)
                        {
                            if (safe != nullptr && picked > 0)
                                safe->choosePreset (picked - 1);
                        });
}

std::unique_ptr<ClicksEditor::Knob> ClicksEditor::addKnob (const juce::String& id, const juce::String& name,
                                                         const juce::String& hint, const juce::String& tip)
{
    auto knob = std::make_unique<Knob>();
    auto watch = [this, tip] (auto& c)
    {
        c.setTooltip (tip);
        c.addMouseListener (this, true);
    };

    knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    knob->slider.getProperties().set ("paramId", id);
    knob->slider.getProperties().set ("face", knobFaceFor (id));
    knob->slider.setColour (juce::Slider::textBoxTextColourId, kPencil);
    knob->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    knob->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob->slider.setLookAndFeel (&look);
    knob->slider.setName (name);
    watch (knob->slider);
    addAndMakeVisible (knob->slider);

    knob->label.setText (name, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setFont (chicagoFont (15.f));
    knob->label.setColour (juce::Label::textColourId, kPencil);
    knob->label.setLookAndFeel (&look);
    watch (knob->label);
    addAndMakeVisible (knob->label);

    knob->hint.setText (hint, juce::dontSendNotification);
    knob->hint.setJustificationType (juce::Justification::centred);
    knob->hint.setFont (chicagoFont (12.f));
    knob->hint.setMinimumHorizontalScale (0.6f);
    knob->hint.setColour (juce::Label::textColourId, kPencil.withAlpha (0.88f));
    knob->hint.setLookAndFeel (&look);
    knob->hint.setVisible (false);
    addAndMakeVisible (knob->hint);

    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        clicks.apvts, id, knob->slider);
    return knob;
}

std::unique_ptr<ClicksEditor::Knob> ClicksEditor::addFader (const juce::String& id, const juce::String& name,
                                                           const juce::String& hint, const juce::String& tip)
{
    auto fader = addKnob (id, name, hint, tip);
    fader->slider.setSliderStyle (juce::Slider::LinearVertical);
    fader->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 36, 14);
    return fader;
}

void ClicksEditor::placeRow (std::vector<std::unique_ptr<Knob>>& knobs, int begin, int count,
                             int columns, juce::Rectangle<int> row)
{
    if ((int) knobs.size() < begin + count || count <= 0 || columns <= 0)
        return;
    const int col = row.getWidth() / columns;
    for (int i = 0; i < count; ++i)
    {
        auto* knob = knobs[(size_t) (begin + i)].get();
        if (knob == nullptr)
            continue;
        auto cell = row.removeFromLeft (col).reduced (1, 0);
        knob->label.setBounds (cell.removeFromTop (14));
        knob->hint.setBounds ({});
        knob->slider.setBounds (cell);
    }
}

// Starfield, title wash, and the three paper sheets. Drawn once per layout
// and blitted in paint(), so the big scans are resampled once.
// Flip every channel in place. ARGB is premultiplied, so the inverse of a
// colour under alpha a is a minus the stored value.
static void invertPixels (juce::Image& image)
{
    juce::Image::BitmapData px (image, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < px.height; ++y)
    {
        if (px.pixelFormat == juce::Image::ARGB)
        {
            for (int x = 0; x < px.width; ++x)
            {
                auto* p = reinterpret_cast<juce::PixelARGB*> (px.getPixelPointer (x, y));
                const int a = p->getAlpha();
                p->setARGB ((juce::uint8) a,
                            (juce::uint8) juce::jmax (0, a - (int) p->getRed()),
                            (juce::uint8) juce::jmax (0, a - (int) p->getGreen()),
                            (juce::uint8) juce::jmax (0, a - (int) p->getBlue()));
            }
        }
        else if (px.pixelFormat == juce::Image::RGB)
        {
            for (int x = 0; x < px.width; ++x)
            {
                auto* p = reinterpret_cast<juce::PixelRGB*> (px.getPixelPointer (x, y));
                p->setARGB (255, (juce::uint8) (255 - p->getRed()),
                            (juce::uint8) (255 - p->getGreen()),
                            (juce::uint8) (255 - p->getBlue()));
            }
        }
    }
}

void ClicksEditor::bakeBackdrop()
{
    const int w = getWidth();
    const int h = getHeight();
    if (w < 1 || h < 1)
        return;
    // The software image type keeps every pixel under our control. The
    // native (CoreGraphics) draw left the scanned sheets see-through in the
    // host, so the sheets below are written straight into the pixels.
    backdrop = juce::Image (juce::SoftwareImageType().create (juce::Image::ARGB, w, h, true));
    {
        juce::Graphics g (backdrop);

        g.fillAll (juce::Colours::black);
        if (field.isValid())
            g.drawImage (field, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);

        if (! titleBand.isEmpty())
        {
            auto band = titleBand;
            if (! initButton.getBounds().isEmpty())
                band.setBottom (initButton.getBottom() + 2);
            g.setColour (juce::Colours::black.withAlpha (0.42f));
            g.fillRect (band);
        }
    }

    // Dark mode flips the whole face at paint time. The stars are already
    // dark, so they go in flipped here and come out as drawn.
    if (dark)
        invertPixels (backdrop);

    {
        juce::Graphics g (backdrop);

        // Drawn card for a missing scan.
        auto card = [&] (juce::Rectangle<int> panel, const juce::Image& plate, uint32_t salt)
        {
            if (panel.isEmpty() || plate.isValid())
                return;
            const auto shape = wobbleRect (panel.toFloat(), salt);
            g.setColour (kPaper);
            g.fillPath (shape);
            g.setTiledImageFill (grain, 0.f, 0.f, 0.85f);
            g.fillPath (shape);
            g.setColour (kPencil);
            g.strokePath (shape, juce::PathStrokeType (1.8f));
        };
        card (genPanel, look.panelMain, 0x61u);
        card (filterPanel, look.panelMain, 0x9Au);
        card (outPanel, look.panelOut, 0x2Bu);
    }

    // Shrink the scan to the panel, a box average per pixel, and blend it
    // into the backdrop by hand with a hard alpha: the drawing is solid
    // paper, the clear margin around it stays clear, and only a one pixel
    // rim blends with the stars.
    auto sheet = [&] (juce::Rectangle<int> panel, const juce::Image& plate)
    {
        if (panel.isEmpty() || ! plate.isValid())
            return;
        const bool wide = panel == outPanel;
        const float cropL = wide ? 0.0297f : 0.0211f;
        const float cropR = wide ? 0.0309f : 0.0211f;
        const float cropT = wide ? 0.1031f : 0.0178f;
        const float cropB = wide ? 0.1147f : 0.0222f;
        const int sw = plate.getWidth();
        const int sh = plate.getHeight();
        const juce::Rectangle<int> src ((int) std::lround ((float) sw * cropL),
                                        (int) std::lround ((float) sh * cropT),
                                        juce::jmax (1, (int) std::lround ((float) sw * (1.f - cropL - cropR))),
                                        juce::jmax (1, (int) std::lround ((float) sh * (1.f - cropT - cropB))));
        const int dw = panel.getWidth();
        const int dh = panel.getHeight();
        juce::Image::BitmapData in (plate, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData out (backdrop, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < dh; ++y)
        {
            const int oy = panel.getY() + y;
            if (oy < 0 || oy >= out.height)
                continue;
            const int y0 = src.getY() + (int) ((int64_t) y * src.getHeight() / dh);
            const int y1 = juce::jmax (y0 + 1, src.getY() + (int) ((int64_t) (y + 1) * src.getHeight() / dh));
            for (int x = 0; x < dw; ++x)
            {
                const int ox = panel.getX() + x;
                if (ox < 0 || ox >= out.width)
                    continue;
                const int x0 = src.getX() + (int) ((int64_t) x * src.getWidth() / dw);
                const int x1 = juce::jmax (x0 + 1, src.getX() + (int) ((int64_t) (x + 1) * src.getWidth() / dw));
                uint32_t r = 0, gr = 0, b = 0, a = 0, n = 0;
                for (int yy = y0; yy < y1 && yy < in.height; ++yy)
                    for (int xx = x0; xx < x1 && xx < in.width; ++xx)
                    {
                        const auto c = in.getPixelColour (xx, yy);
                        const uint32_t ca = c.getAlpha();
                        a += ca;
                        r += c.getRed() * ca;
                        gr += c.getGreen() * ca;
                        b += c.getBlue() * ca;
                        ++n;
                    }
                if (n == 0 || a == 0)
                    continue;
                const float cover = (float) a / (255.f * (float) n);
                const float hard = juce::jlimit (0.f, 1.f, (cover - 0.35f) / 0.3f);
                if (hard <= 0.f)
                    continue;
                const juce::Colour ink ((juce::uint8) (r / a), (juce::uint8) (gr / a), (juce::uint8) (b / a));
                if (hard >= 1.f)
                    out.setPixelColour (ox, oy, ink);
                else
                    out.setPixelColour (ox, oy, out.getPixelColour (ox, oy).interpolatedWith (ink, hard));
            }
        }
    };
    sheet (genPanel, look.panelMain);
    sheet (filterPanel, look.panelMain);
    sheet (outPanel, look.panelOut);
}

void ClicksEditor::Invert::applyEffect (juce::Image& image, juce::Graphics& g, float, float alpha)
{
    invertPixels (image);
    g.setOpacity (alpha);
    g.drawImageAt (image, 0, 0);
}

void ClicksEditor::applyTheme()
{
    darkButton.setButtonText (dark ? "light" : "dark");
    for (auto* child : getChildren())
        child->setComponentEffect (dark ? &invert : nullptr);
    // Menus open in their own window, outside the effect, so their colours flip here.
    auto ink = [this] (juce::Colour c)
    {
        return dark ? juce::Colour::fromRGBA ((juce::uint8) (255 - c.getRed()), (juce::uint8) (255 - c.getGreen()),
                                              (juce::uint8) (255 - c.getBlue()), c.getAlpha())
                    : c;
    };
    look.setColour (juce::PopupMenu::backgroundColourId, ink (kPaper));
    look.setColour (juce::PopupMenu::textColourId, ink (kPencil));
    look.setColour (juce::PopupMenu::highlightedBackgroundColourId, ink (juce::Colour (0xffddd4c4)));
    look.setColour (juce::PopupMenu::highlightedTextColourId, ink (kPencil));
    bakeBackdrop();
    repaint();
}

// Draw the dirty part of the face into a plate at screen resolution, flip
// it, and blit it back. The plate is only as big as the region being
// repainted, so the 30 Hz scope and meter updates stay cheap.
void ClicksEditor::paintInverted (juce::Graphics& g)
{
    const auto clip = g.getClipBounds();
    if (clip.isEmpty())
        return;
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int pw = juce::jmax (1, (int) std::ceil ((float) clip.getWidth() * scale));
    const int ph = juce::jmax (1, (int) std::ceil ((float) clip.getHeight() * scale));
    if (! darkPlate.isValid() || darkPlate.getWidth() != pw || darkPlate.getHeight() != ph)
        darkPlate = juce::Image (juce::Image::ARGB, pw, ph, true);
    else
        darkPlate.clear (darkPlate.getBounds());
    {
        juce::Graphics g2 (darkPlate);
        g2.addTransform (juce::AffineTransform::translation ((float) -clip.getX(), (float) -clip.getY())
                             .scaled (scale));
        paintFace (g2);
    }
    invertPixels (darkPlate);
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::scale (1.f / scale));
    g.drawImageAt (darkPlate, (int) std::lround ((float) clip.getX() * scale),
                   (int) std::lround ((float) clip.getY() * scale));
}

void ClicksEditor::paintShadows (juce::Graphics& g)
{
    for (auto* child : getChildren())
    {
        if (child == nullptr || ! child->isVisible() || child == &presetBox)
            continue;
        if (auto* slider = dynamic_cast<juce::Slider*> (child))
        {
            auto layout = look.getSliderLayout (*slider);
            const auto origin = slider->getPosition().toFloat();
            if (slider->isRotary())
            {
                auto local = layout.sliderBounds.toFloat();
                const float side = juce::jmin (local.getWidth(), local.getHeight());
                auto plate = local.withSizeKeepingCentre (side, side) + origin;
                const int art = (int) slider->getProperties().getWithDefault ("face", 0);
                if (art < 0 || art >= 9 || ! look.knob[art].isValid())
                    continue;
                float pos = (float) slider->valueToProportionOfLength (slider->getValue());
                const float drive = (float) slider->getProperties().getWithDefault ("lfoDrive", 0.0);
                pos = juce::jlimit (0.f, 1.f, pos + drive * 0.5f);
                const auto spin = slider->getRotaryParameters();
                const float rot = (pos - 0.5f) * (spin.endAngleRadians - spin.startAngleRadians);
                castArt (g, look.knob[art], plate, rot, false);
            }
            else if (look.faderCap.isValid())
            {
                auto track = layout.sliderBounds;
                auto area = track.toFloat().reduced (2.f);
                const bool vertical = slider->isVertical();
                const float prop = (float) slider->valueToProportionOfLength (slider->getValue());
                const float drive = (float) slider->getProperties().getWithDefault ("lfoDrive", 0.0);
                const float shown = juce::jlimit (0.f, 1.f, prop + drive * 0.5f);
                const float travel = vertical ? (float) track.getHeight() : (float) track.getWidth();
                float sliderPos = (vertical ? (float) track.getY() : (float) track.getX())
                                + (vertical ? 1.f - prop : prop) * travel;
                const float delta = (shown - prop) * (vertical ? area.getHeight() : area.getWidth());
                sliderPos += vertical ? -delta : delta;
                const float cap = vertical ? area.getWidth()
                                           : juce::jmin (area.getHeight() * 1.2f, 34.f);
                auto head = vertical
                    ? juce::Rectangle<float> (area.getX(), sliderPos - cap * 0.5f, area.getWidth(), cap)
                    : juce::Rectangle<float> (sliderPos - cap * 0.5f, area.getY(), cap, area.getHeight());
                castArt (g, look.faderCap, head + origin, 0.f, false);
            }
        }
        else if (auto* box = dynamic_cast<juce::ComboBox*> (child))
        {
            if (look.barFill.isValid())
                castArt (g, look.barFill, box->getBounds().toFloat(), 0.f, false);
            else
                castRect (g, box->getBounds().toFloat());
        }
        else if (auto* toggle = dynamic_cast<juce::ToggleButton*> (child))
        {
            const bool pair = (int) toggle->getProperties().getWithDefault ("pair", 0) != 0;
            const bool flipped = pair ? ! toggle->getToggleState() : toggle->getToggleState();
            if (look.switchArt.isValid())
                castArt (g, look.switchArt, toggle->getBounds().toFloat().reduced (0.5f), 0.f, flipped);
            else
                castRect (g, toggle->getBounds().toFloat());
        }
        else if (dynamic_cast<juce::TextButton*> (child) != nullptr
                 || dynamic_cast<juce::TextEditor*> (child) != nullptr)
        {
            castRect (g, child->getBounds().toFloat());
        }
    }
    if (presetBox.isVisible())
        castRect (g, presetBox.getBounds().toFloat());
}

void ClicksEditor::paint (juce::Graphics& g)
{
    if (dark)
        paintInverted (g);
    else
        paintFace (g);
}

void ClicksEditor::paintFace (juce::Graphics& g)
{
    if (backdrop.isValid() && backdrop.getWidth() == getWidth() && backdrop.getHeight() == getHeight())
        g.drawImageAt (backdrop, 0, 0);
    else if (field.isValid())
        g.drawImage (field, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);
    else
        g.fillAll (juce::Colours::black);

    paintShadows (g);

    if (! scopePanel.isEmpty())
    {
        const int w = std::max (1, scopePanel.getWidth());
        const int h = std::max (1, scopePanel.getHeight());
        // Bake the wordmark once and keep the PNG's transparency. The plate is
        // cleared, so a later scope repaint does not stack another copy on top.
        if (logoPlate.isNull() || logoPlate.getWidth() != w || logoPlate.getHeight() != h)
        {
            logoPlate = juce::Image (juce::Image::ARGB, w, h, true);
            if (mark.isValid())
            {
                juce::Graphics ig (logoPlate);
                ig.drawImage (mark, juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h),
                              juce::RectanglePlacement::stretchToFit);
            }
            juce::Image::BitmapData pix (logoPlate, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    const auto c = pix.getPixelColour (x, y);
                    const float a = c.getFloatAlpha();
                    if (a < 0.04f)
                    {
                        pix.setPixelColour (x, y, juce::Colours::transparentBlack);
                        continue;
                    }
                    auto punch = [] (float v)
                    {
                        v = (v - 0.45f) * 1.35f + 0.45f;
                        return juce::jlimit (0.f, 1.f, v);
                    };
                    const float r = punch (c.getFloatRed());
                    const float gc = punch (c.getFloatGreen());
                    const float b = punch (c.getFloatBlue());
                    pix.setPixelColour (x, y, juce::Colour::fromFloatRGBA (r, gc, b, a));
                }
            }
        }
    }

    // The sheets are in the backdrop. Only the titles are live here.
    auto paper = [&] (juce::Rectangle<int> panel, const juce::String& title, int titleDrop)
    {
        if (panel.isEmpty())
            return;
        g.setColour (kPencil);
        g.setFont (chicagoFont (26.f));
        g.drawText (title, panel.reduced (26, 0).withTrimmedTop (titleDrop).removeFromTop (30),
                    juce::Justification::centredLeft);
    };
    paper (genPanel, "source", 16);
    paper (filterPanel, "effects", 16);
    paper (outPanel, "output", juce::jmax (16, outPanel.getHeight() * 11 / 100));

    if (! presetBox.getBounds().isEmpty())
    {
        auto box = presetBox.getBounds().toFloat();
        g.setColour (kPaper);
        g.fillRect (box);
        g.setColour (kPencil);
        g.drawRect (box, 1.15f);
    }

    if (logoPlate.isValid() && ! scopePanel.isEmpty())
    {
        g.drawImageAt (logoPlate, scopePanel.getX(), scopePanel.getY());
        if (scopeTrace.size() >= 2)
        {
            const auto plot = scopePanel.reduced (2, 2);
            const float mid = (float) plot.getCentreY();
            const float amp = (float) plot.getHeight() * 0.46f;
            juce::Path trace;
            const int n = (int) scopeTrace.size();
            for (int i = 0; i < n; ++i)
            {
                const float x = (float) plot.getX() + (float) plot.getWidth() * (float) i / (float) (n - 1);
                const float y = mid - scopeTrace[(size_t) i] * amp;
                if (i == 0)
                    trace.startNewSubPath (x, y);
                else
                    trace.lineTo (x, y);
            }
            g.setColour (juce::Colour (0xffff4ad0).withAlpha (0.55f));
            g.strokePath (trace, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xff3dfff0));
            g.strokePath (trace, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }
    }

    auto heading = [&] (juce::Rectangle<int> r, const juce::String& text, float alpha, float pt = 22.f)
    {
        if (r.isEmpty())
            return;
        g.setColour (kPencil.withAlpha (alpha));
        g.setFont (chicagoFont (pt));
        g.drawText (text, r, juce::Justification::centredLeft);
    };
    const int wavePick = (int) std::lround (clicks.apvts.getRawParameterValue ("wave")->load());
    const bool samOn = wavePick >= 6;
    const int filtType = (int) clicks.apvts.getRawParameterValue (filtPage == 0 ? "filttype" : "filt2type")->load();
    auto frame = [&] (juce::Rectangle<int> r)
    {
        if (r.isEmpty())
            return;
        g.setColour (kPencil.withAlpha (0.55f));
        g.drawRect (r.toFloat().reduced (0.5f), 1.3f);
    };
    frame (tapeFrame);
    frame (lfoPanel);
    frame (subFrame);
    frame (filtFrame);
    frame (formFrame);
    frame (ringFrame);
    frame (crushFrame);
    frame (delayFrame);
    frame (revFrame);
    frame (compFrame);
    if (look.tapeDeck.isValid() && tapeArt.getWidth() > 36 && tapeArt.getHeight() > 36)
    {
        const auto& img = look.tapeDeck;
        const auto area = tapeArt.toFloat();
        const float aspect = (float) img.getWidth() / (float) juce::jmax (1, img.getHeight());
        float h = area.getHeight();
        float w = h * aspect;
        if (w > area.getWidth())
        {
            w = area.getWidth();
            h = w / aspect;
        }
        g.setOpacity (0.9f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, juce::Rectangle<float> (area.getCentreX() - w * 0.5f,
                                                  area.getCentreY() - h * 0.5f, w, h),
                     juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.f);
    }
    auto drawWave = [&] (juce::Rectangle<int> plate, int slot, bool falling)
    {
        if (plate.getWidth() < 24 || plate.getHeight() < 16 || slot < 0 || slot > 4)
            return;
        const auto& img = look.wave[slot];
        if (! img.isValid())
            return;
        auto area = plate.toFloat();
        const float aspect = (float) img.getWidth() / (float) juce::jmax (1, img.getHeight());
        float h = area.getHeight();
        float w = h * aspect;
        if (w > area.getWidth())
        {
            w = area.getWidth();
            h = w / aspect;
        }
        const float x = area.getCentreX() - w * 0.5f;
        const float y = area.getCentreY() - h * 0.5f;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        if (falling)
        {
            g.saveState();
            g.addTransform (juce::AffineTransform::verticalFlip (y + h * 0.5f));
        }
        g.drawImage (img, juce::Rectangle<float> (x, y, w, h), juce::RectanglePlacement::stretchToFit);
        if (falling)
            g.restoreState();
    };
    const int wavePickArt = (int) std::lround (clicks.apvts.getRawParameterValue ("wave")->load());
    const int subPickArt = (int) std::lround (clicks.apvts.getRawParameterValue ("subwave")->load());
    static const int waveArtMap[] = { 0, 1, 1, 2, 3, 4 };
    static const int subArtMap[] = { 0, 2, 1, 1, 3 };
    const int mainArt = wavePickArt >= 0 && wavePickArt < 6 ? waveArtMap[wavePickArt] : -1;
    const int subArt = subPickArt >= 0 && subPickArt < 5 ? subArtMap[subPickArt] : -1;
    drawWave (wavePlate, mainArt, wavePickArt == 2);
    drawWave (subWavePlate, subArt, subPickArt == 3);
    heading (playTag, "tape", 0.9f, 22.f);
    heading (lfoTag, "lfo", 0.9f, 22.f);
    heading (samTag, wavePick == 7 ? "speak" : "sam", samOn ? 0.9f : 0.35f);
    heading (subTag, "sub oscillator", 0.9f, 22.f);
    heading (crushTag, "crush", 0.9f, 22.f);
    heading (envTag, "source", 0.9f);
    heading (subEnvTag, "sub", 0.9f);
    heading (filtTag, "filter", 0.9f);
    heading (formantTag, "formant", filtType == 5 ? 0.9f : 0.35f);
    heading (ringTag, "resonator", filtType == 6 ? 0.9f : 0.35f);
    heading (filtEnvTag, "env", 0.9f);
    heading (delayTag, "delay", 0.9f);
    heading (revTag, "reverb", 0.9f);
    heading (compTag, "compress", 0.9f);
    auto holdPlate = [&] (juce::Rectangle<int> r)
    {
        if (r.isEmpty())
            return;
        auto box = r.toFloat();
        if (look.switchArt.isValid())
        {
            g.setOpacity (0.55f);
            g.drawImage (look.switchArt, box, juce::RectanglePlacement::stretchToFit);
            g.setOpacity (1.f);
        }
        else
        {
            g.setColour (kPaper);
            g.fillRect (box);
            g.setColour (kPencil);
            g.drawRect (box, 1.15f);
            const float y = box.getCentreY();
            g.drawLine (box.getX() + 10.f, y, box.getRight() - 10.f, y, 1.6f);
        }
    };
    holdPlate (envHoldPlate);
    holdPlate (subHoldPlate);
    holdPlate (filtHoldPlate);
    if (! meterBounds.isEmpty())
    {
        auto bar = meterBounds.toFloat();
        const float floorDb = -48.f;
        auto dbOf = [] (float lin)
        {
            return lin < 1.0e-5f ? -60.f : 20.f * std::log10 (lin);
        };
        auto yOf = [&] (float db)
        {
            const float t = juce::jlimit (0.f, 1.f, (db - floorDb) / -floorDb);
            return bar.getBottom() - t * bar.getHeight();
        };
        g.setColour (kPaper);
        g.fillRect (bar);
        g.setColour (kPencil);
        g.drawRect (bar, 1.2f);
        const float levelY = yOf (dbOf (meterShown));
        if (levelY < bar.getBottom() - 1.f)
        {
            if (look.barFill.isValid())
            {
                g.saveState();
                g.reduceClipRegion (juce::Rectangle<int> ((int) std::floor (bar.getX()),
                                                         (int) std::floor (levelY),
                                                         (int) std::ceil (bar.getWidth()) + 1,
                                                         (int) std::ceil (bar.getBottom() - levelY) + 1));
                const auto c = bar.getCentre();
                g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi, c.x, c.y));
                auto spun = juce::Rectangle<float> (c.x, c.y, 0.f, 0.f)
                                .withSizeKeepingCentre (bar.getHeight(), bar.getWidth());
                g.drawImage (look.barFill, spun, juce::RectanglePlacement::stretchToFit);
                g.restoreState();
            }
            else
            {
                g.setColour (kPencil);
                g.fillRect (bar.getX() + 2.f, levelY, bar.getWidth() - 4.f, bar.getBottom() - levelY - 1.f);
            }
        }
        const float ceilDb = clicks.apvts.getRawParameterValue ("limit")->load();
        const float ceilY = yOf (ceilDb);
        g.setColour (juce::Colour (0xff3dfff0));
        g.fillRect (bar.getX() - 3.f, ceilY - 1.f, bar.getWidth() + 6.f, 2.f);
        const float holdY = yOf (dbOf (meterHold));
        g.setColour (kPencil);
        g.fillRect (bar.getX() + 2.f, holdY, bar.getWidth() - 4.f, 1.5f);
        g.setFont (chicagoFont (15.f));
        g.drawText ("out", outMeter.getBounds().removeFromTop (16), juce::Justification::centred);
    }

    if (bot.isValid() && ! botPanel.isEmpty())
    {
        const auto face = botImageBounds();
        const int fw = std::max (1, (int) std::lround (face.getWidth()));
        const int fh = std::max (1, (int) std::lround (face.getHeight()));
        if (botPlate.isNull() || botPlate.getWidth() != fw || botPlate.getHeight() != fh)
        {
            botPlate = juce::Image (juce::Image::ARGB, fw, fh, true);
            botWash = juce::Image (juce::Image::ARGB, fw, fh, true);
            {
                juce::Graphics ig (botPlate);
                ig.drawImage (bot, juce::Rectangle<float> (0.f, 0.f, (float) fw, (float) fh),
                              juce::RectanglePlacement::stretchToFit);
            }
            juce::Image::BitmapData pix (botPlate, juce::Image::BitmapData::readWrite);
            juce::Image::BitmapData wash (botWash, juce::Image::BitmapData::writeOnly);
            const float feather = 18.f;
            for (int y = 0; y < fh; ++y)
            {
                for (int x = 0; x < fw; ++x)
                {
                    const float dx = (float) std::min (x, fw - 1 - x);
                    const float dy = (float) std::min (y, fh - 1 - y);
                    float t = juce::jlimit (0.f, 1.f, std::min (dx, dy) / feather);
                    t = t * t * (3.f - 2.f * t);
                    const auto c = pix.getPixelColour (x, y);
                    pix.setPixelColour (x, y, c.withAlpha (c.getFloatAlpha() * t));
                    wash.setPixelColour (x, y, kPaper.withAlpha (0.62f * t));
                }
            }
        }
        g.drawImage (botPlate, face, juce::RectanglePlacement::stretchToFit);

        // Mouth screen of the robot picture. The painted wave is covered so the
        // trace here is only the live Sam voice.
        const float ix0 = 0.145f, iy0 = 0.668f, ix1 = 0.825f, iy1 = 0.900f;
        auto mouth = juce::Rectangle<float> (face.getX() + face.getWidth() * ix0,
                                              face.getY() + face.getHeight() * iy0,
                                              face.getWidth() * (ix1 - ix0),
                                              face.getHeight() * (iy1 - iy0));
        g.setColour (juce::Colour (0xff07080c));
        g.fillRect (mouth);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine ((int) (mouth.getY() + mouth.getHeight() * (float) i / 4.f),
                                  mouth.getX(), mouth.getRight());
        for (int i = 1; i < 6; ++i)
            g.drawVerticalLine ((int) (mouth.getX() + mouth.getWidth() * (float) i / 6.f),
                                mouth.getY(), mouth.getBottom());

        float peak = 0.f;
        for (float s : samTrace)
            peak = std::max (peak, std::abs (s));
        if (samOn && samTrace.size() >= 2 && peak > 0.02f)
        {
            juce::Path trace;
            const int n = (int) samTrace.size();
            const float mid = mouth.getCentreY();
            const float amp = mouth.getHeight() * 0.42f;
            for (int i = 0; i < n; ++i)
            {
                const float x = mouth.getX() + mouth.getWidth() * (float) i / (float) (n - 1);
                const float y = mid - samTrace[(size_t) i] * amp;
                if (i == 0)
                    trace.startNewSubPath (x, y);
                else
                    trace.lineTo (x, y);
            }
            g.setColour (juce::Colour (0xff7dffe0).withAlpha (0.45f));
            g.strokePath (trace, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xffd8fff4));
            g.strokePath (trace, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }
        if (! samOn)
            g.drawImage (botWash, face, juce::RectanglePlacement::stretchToFit);
    }
}

juce::Rectangle<float> ClicksEditor::botImageBounds() const
{
    if (! bot.isValid() || botPanel.isEmpty())
        return {};
    return juce::RectanglePlacement (juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize)
        .appliedTo (bot.getBounds().toFloat(), botPanel.toFloat());
}

void ClicksEditor::paintOverChildren (juce::Graphics&)
{
}

void ClicksEditor::resized()
{
    const int wantW = 1216;
    const int wantH = 1082;
    if (getWidth() != wantW || getHeight() != wantH)
    {
        setSize (wantW, wantH);
        return;
    }

    auto area = getLocalBounds().reduced (8);
    const int gap = 14;
    const int colW = (area.getWidth() - gap) / 2;
    auto left = area.removeFromLeft (colW);
    area.removeFromLeft (gap);

    float aspect = 800.f / 205.f;
    if (mark.isValid() && mark.getHeight() > 0)
        aspect = (float) mark.getWidth() / (float) mark.getHeight();
    const int logoW = area.getWidth();
    const int logoH = std::max (1, (int) std::lround ((float) logoW / std::max (0.01f, aspect)));
    scopePanel = juce::Rectangle<int> (area.getX(), area.getY(), logoW, logoH);

    auto bar = left.removeFromTop (24);
    titleBand = bar;
    left.removeFromTop (gap);
    initButton.setBounds (bar.removeFromLeft (64).reduced (0, 1));
    bar.removeFromLeft (4);
    saveButton.setBounds (bar.removeFromLeft (64).reduced (0, 1));
    bar.removeFromLeft (4);
    loadButton.setBounds (bar.removeFromLeft (64).reduced (0, 1));
    bar.removeFromLeft (8);
    prevButton.setBounds (bar.removeFromLeft (24).reduced (0, 1));
    bar.removeFromLeft (2);
    darkButton.setBounds (bar.removeFromRight (56).reduced (0, 1));
    bar.removeFromRight (8);
    nextButton.setBounds (bar.removeFromRight (24).reduced (0, 1));
    bar.removeFromRight (2);
    presetBox.setBounds (bar.reduced (0, 1));

    const int dropH = 28;
    genPanel = left.removeFromTop (848);
    left.removeFromTop (gap);
    outPanel = left;
    area.removeFromTop (logoH + gap);
    filterPanel = area;

    auto placeDrop = [] (juce::Label& caption, juce::ComboBox& box, juce::Rectangle<int> bounds)
    {
        caption.setBounds (bounds.removeFromLeft (juce::jmin (72, bounds.getWidth() / 3)));
        box.setBounds (bounds.reduced (0, 1));
    };
    auto placeTight = [&] (std::vector<std::unique_ptr<Knob>>& knobs, int begin, int count,
                           int cellW, juce::Rectangle<int> row)
    {
        row = row.removeFromLeft (juce::jmin (row.getWidth(), cellW * count));
        placeRow (knobs, begin, count, count, row);
    };

    // Content stays inside the pencil frame of the scan.
    auto gen = genPanel.withTrimmedLeft (18).withTrimmedRight (19)
                       .withTrimmedTop (14).withTrimmedBottom (22);
    gen.removeFromTop (42);
    auto placeProminent = [this] (Knob& knob, juce::Rectangle<int> cell, float title)
    {
        cell = cell.reduced (1, 0);
        knob.label.setFont (chicagoFont (title));
        knob.label.setBounds (cell.removeFromTop ((int) title + 4));
        knob.hint.setBounds ({});
        knob.slider.setBounds (cell);
    };

    auto placeCells = [] (std::initializer_list<Knob*> knobs, juce::Rectangle<int> row)
    {
        const int n = (int) knobs.size();
        if (n <= 0)
            return;
        const int col = row.getWidth() / n;
        for (auto* knob : knobs)
        {
            auto cell = row.removeFromLeft (col);
            if (knob == nullptr)
                continue;
            knob->label.setBounds (cell.removeFromTop (14));
            knob->hint.setBounds ({});
            knob->slider.setBounds (cell);
        }
    };

    {
        const int faceW = 156;
        auto band = gen.removeFromTop (160);
        botPanel = band.removeFromRight (faceW).reduced (6, 2);
        auto waveRow = band.removeFromTop (120);
        if (generator.size() > 10 && generator[10] != nullptr)
            placeProminent (*generator[10], waveRow.removeFromLeft (120), 15.f);
        if (generator.size() > 14 && generator[14] != nullptr)
            placeProminent (*generator[14], waveRow.removeFromLeft (100), 15.f);
        auto lists = waveRow.reduced (4, 0);
        wavePlate = lists.removeFromTop (58).reduced (0, 2);
        lists.removeFromTop (2);
        placeDrop (waveLabel, waveBox, lists.removeFromTop (dropH));
        lists.removeFromTop (4);
        placeDrop (polyLabel, polyBox, lists.removeFromTop (dropH));
        samTag = band.removeFromTop (22);
        auto sayRow = band;
        sayLabel.setBounds (sayRow.removeFromLeft (36));
        lockButton.setBounds (sayRow.removeFromRight (64).reduced (0, 1));
        sayRow.removeFromRight (4);
        sayBox.setBounds (sayRow.reduced (0, 1));
    }
    {
        const auto voiceRow = gen.removeFromTop (96);
        placeTight (sam, 0, 5, 96, voiceRow);
        placeTight (speak, 0, 5, 96, voiceRow);
    }

    subFrame = gen.removeFromTop (146);
    {
        auto box = subFrame.reduced (6, 4);
        subTag = box.removeFromTop (24);
        auto lists = box.removeFromLeft (176);
        placeDrop (subLabel, subBox, lists.removeFromTop (dropH));
        lists.removeFromTop (4);
        placeDrop (subWaveLabel, subWaveBox, lists.removeFromTop (dropH));
        subWavePlate = lists.reduced (2, 2);
        placeCells ({ generator.size() > 1 ? generator[1].get() : nullptr,
                      generator.size() > 0 ? generator[0].get() : nullptr,
                      generator.size() > 11 ? generator[11].get() : nullptr,
                      generator.size() > 12 ? generator[12].get() : nullptr },
                    box);
    }

    auto layFlat = [] (std::vector<std::unique_ptr<Knob>>& faders, juce::Rectangle<int> col,
                       juce::ToggleButton* attack, juce::ToggleButton* decay, juce::ToggleButton* release,
                       juce::Rectangle<int>* holdPlate)
    {
        juce::ToggleButton* ticks[5] = { attack, decay, nullptr, release, nullptr };
        if (holdPlate != nullptr)
            *holdPlate = {};
        const int rows = juce::jmin (5, (int) faders.size());
        if (rows <= 0)
            return;
        const int h = std::max (20, col.getHeight() / rows);
        for (int i = 0; i < rows; ++i)
        {
            auto* fader = faders[(size_t) i].get();
            if (fader == nullptr)
                continue;
            auto line = col.removeFromTop (h);
            fader->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            fader->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 14);
            fader->label.setBounds (line.removeFromLeft (36));
            fader->hint.setBounds ({});
            auto slot = line.removeFromRight (78).reduced (1, 3);
            if (ticks[i] != nullptr)
                ticks[i]->setBounds (slot);
            else if (i == 2 && holdPlate != nullptr)
                *holdPlate = slot;
            fader->slider.setBounds (line.reduced (2, 2));
        }
    };

    {
        auto row = gen.removeFromTop (156);
        auto leftEnv = row.removeFromLeft (row.getWidth() / 2).reduced (4, 0);
        auto rightEnv = row.reduced (4, 0);
        envTag = leftEnv.removeFromTop (22);
        subEnvTag = rightEnv.removeFromTop (22);
        layFlat (env, leftEnv, &attackSync, &decaySync, &releaseSync, &envHoldPlate);
        layFlat (senv, rightEnv, &subAttackSync, &subDecaySync, &subReleaseSync, &subHoldPlate);
    }

    lfoPanel = gen;

    auto filt = filterPanel.withTrimmedLeft (18).withTrimmedRight (19)
                           .withTrimmedTop (14).withTrimmedBottom (22);
    filt.removeFromTop (42);
    {
        const int avail = filt.getHeight();
        int tapeH = 300;
        int filtH = 136;
        int pairH = 86;
        int crushH = 84;
        int delayH = 100;
        int compH = 100;
        const int over = tapeH + filtH + pairH + crushH + delayH + compH - avail;
        if (over > 0)
            tapeH = juce::jmax (236, tapeH - over);

        // Chain order on the panel: filter, formant and ring, crush, delay and
        // reverb, then the tape just above the compressor.
        filtFrame = filt.removeFromTop (filtH);
    {
        auto box = filtFrame.reduced (4, 2);
        auto envCol = box.removeFromRight (230);
        filtEnvTag = envCol.removeFromTop (22);
        auto envBody = envCol;
        layFlat (fenv, envCol, &fAttackSync, &fDecaySync, &fReleaseSync, &filtHoldPlate);
        layFlat (fenv2, envBody, &f2AttackSync, &f2DecaySync, &f2ReleaseSync, &filtHoldPlate);
        filtTag = box.removeFromTop (22);
        auto modes = box.removeFromTop (dropH);
        auto modes2 = modes;
        placeDrop (filtTypeLabel, filtTypeBox, modes.removeFromLeft (140));
        placeDrop (filt2TypeLabel, filt2TypeBox, modes2.removeFromLeft (140));
        auto cutRow = box;
        placeTight (filter, 0, 2, 110, cutRow);
        placeTight (filter2, 0, 3, 100, box);
    }
    {
        auto row = filt.removeFromTop (pairH);
        const int half = juce::jmax (0, (row.getWidth() - 8) / 2);
        formFrame = row.removeFromLeft (half);
        row.removeFromLeft (8);
        ringFrame = row;
        {
            auto box = formFrame.reduced (4, 2);
            formantTag = box.removeFromTop (22);
            auto formRow = box;
            placeTight (filter, 2, 3, 100, formRow);
            placeTight (filter2, 3, 3, 100, box);
        }
        {
            auto box = ringFrame.reduced (4, 2);
            ringTag = box.removeFromTop (22);
            auto ringRow = box;
            placeTight (filter, 5, 3, 100, ringRow);
            placeTight (filter2, 6, 3, 100, box);
        }
    }
    crushFrame = filt.removeFromTop (crushH);
    {
        auto box = crushFrame.reduced (6, 2);
        crushTag = box.removeFromTop (22);
        placeTight (generator, 3, 4, 110, box);
    }
    {
        auto row = filt.removeFromTop (delayH);
        const int half = juce::jmax (0, (row.getWidth() - 8) / 2);
        delayFrame = row.removeFromLeft (half);
        row.removeFromLeft (8);
        revFrame = row;
        {
            auto box = delayFrame.reduced (4, 2);
            delayTag = box.removeFromTop (22);
            placeTight (output, 0, 3, 88, box);
        }
        {
            auto box = revFrame.reduced (4, 2);
            revTag = box.removeFromTop (22);
            placeTight (reverb, 0, 3, 88, box);
        }
    }
    {
        tapeFrame = filt.removeFromTop (tapeH);
        auto tape = tapeFrame.reduced (6, 4);
        playTag = tape.removeFromTop (22);
        const int knobRowH = juce::jmin (86, juce::jmax (64, tape.getHeight() / 3));
        auto knobRow = tape.removeFromBottom (knobRowH);
        tape.removeFromBottom (2);
        const int artLeft = tape.getX() + (tape.getWidth() * 52) / 100;
        tapeArt = juce::Rectangle<int> (artLeft, tape.getY(),
                                        tape.getRight() - artLeft - 2, tape.getHeight()).reduced (2, 2);
        if (generator.size() > 2 && generator[2] != nullptr)
        {
            const int speedW = juce::jmin (360, tape.getWidth());
            const int speedH = juce::jmax (110, tape.getHeight() - (dropH + 2));
            auto speedCol = tape.removeFromTop (speedH).removeFromLeft (speedW);
            auto knobCell = speedCol.removeFromTop (juce::jmax (80, speedCol.getHeight() - 26));
            generator[2]->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
            placeProminent (*generator[2], knobCell, 18.f);
            auto disc = generator[2]->slider.getBounds();
            const int side = juce::jmin (disc.getWidth(), disc.getHeight() + 6);
            generator[2]->slider.setBounds (disc.withSizeKeepingCentre (side, disc.getHeight()));
            speedCol.removeFromTop (2);
            auto syncRow = speedCol.removeFromTop (22).reduced (6, 1);
            syncTick.setBounds (syncRow.removeFromLeft (118));
            syncRow.removeFromLeft (4);
            slowRetrig.setBounds (syncRow.removeFromLeft (46));
            syncRow.removeFromLeft (6);
            pitchTick.setBounds (syncRow.removeFromLeft (140));
        }
        placeDrop (scatLabel, scatBox, tape.removeFromTop (dropH).removeFromLeft (220));
        tape = knobRow;
        if (generator.size() > 8 && generator[8] != nullptr)
            placeProminent (*generator[8], tape.removeFromLeft (juce::jmin (108, tape.getWidth() / 4)), 15.f);
        if (! wobble.empty() && wobble[0] != nullptr)
            placeProminent (*wobble[0], tape.removeFromLeft (juce::jmin (108, tape.getWidth() / 3)), 15.f);
        placeCells ({ generator.size() > 7 ? generator[7].get() : nullptr,
                      generator.size() > 9 ? generator[9].get() : nullptr,
                      generator.size() > 13 ? generator[13].get() : nullptr },
                    tape);
    }
    compFrame = filt.removeFromTop (compH);
    {
        auto box = compFrame.reduced (6, 2);
        compTag = box.removeFromTop (22);
        auto meterCol = box.removeFromRight (44);
        meterCol.removeFromLeft (8);
        outMeter.setBounds (meterCol);
        meterBounds = meterCol.withTrimmedTop (16).reduced (8, 2);
        placeTight (comp, 0, 5, 86, box);
    }
        auto pinSlot = [this] (juce::ComboBox& box, juce::Rectangle<int> tag, const juce::String& title)
        {
            const int textW = juce::GlyphArrangement::getStringWidthInt (chicagoFont (22.f), title);
            const int slotW = 52;
            int x = tag.getX() + textW + 6;
            if (x + slotW > tag.getRight())
                x = juce::jmax (tag.getX(), tag.getRight() - slotW);
            box.setBounds (x, tag.getY(), slotW, tag.getHeight());
        };
        pinSlot (slotBox[0], playTag, "tape");
        pinSlot (slotBox[2], filtTag, "filter");
        {
            const auto slot = slotBox[2].getBounds();
            int x = slot.getRight() + 8;
            for (int i = 0; i < 2; ++i)
            {
                filtTab[i].setBounds (x, filtTag.getY(), 28, filtTag.getHeight());
                filtTab[i].setToggleState (filtPage == i, juce::dontSendNotification);
                x += 32;
            }
        }
        auto showKnob = [] (std::vector<std::unique_ptr<Knob>>& knobs, bool on)
        {
            for (auto& knob : knobs)
                if (knob != nullptr)
                {
                    knob->slider.setVisible (on);
                    knob->label.setVisible (on);
                }
        };
        const bool page1 = filtPage == 0;
        showKnob (filter, page1);
        showKnob (filter2, ! page1);
        showKnob (fenv, page1);
        showKnob (fenv2, ! page1);
        filtTypeLabel.setVisible (page1);
        filtTypeBox.setVisible (page1);
        filt2TypeLabel.setVisible (! page1);
        filt2TypeBox.setVisible (! page1);
        fAttackSync.setVisible (page1);
        fDecaySync.setVisible (page1);
        fReleaseSync.setVisible (page1);
        f2AttackSync.setVisible (! page1);
        f2DecaySync.setVisible (! page1);
        f2ReleaseSync.setVisible (! page1);
        pinSlot (slotBox[1], crushTag, "crush");
        pinSlot (slotBox[3], delayTag, "delay");
        pinSlot (slotBox[4], revTag, "reverb");
        pinSlot (slotBox[5], compTag, "compress");
    }

    {
        const int outTop = juce::jmax (16, outPanel.getHeight() * 11 / 100);
        const int outBot = juce::jmax (10, outPanel.getHeight() * 16 / 100);
        auto out = outPanel.withTrimmedTop (outTop + 32).withTrimmedBottom (outBot)
                           .reduced (40, 0);
        auto sources = out.removeFromLeft (out.getWidth() / 2).reduced (2, 0);
        auto effects = out.reduced (2, 0);
        const int sourceH = std::max (16, sources.getHeight() / 3);
        const int effectH = std::max (16, effects.getHeight() / 4);
        auto layLine = [] (Knob* fader, juce::Rectangle<int> line)
        {
            if (fader == nullptr)
                return;
            fader->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            fader->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 14);
            fader->label.setJustificationType (juce::Justification::centredLeft);
            fader->label.setBounds (line.removeFromLeft (48));
            fader->hint.setBounds ({});
            fader->slider.setBounds (line.reduced (2, 1));
        };
        Knob* sourceRows[3] = {
            mix.size() > 0 ? mix[0].get() : nullptr,
            mix.size() > 1 ? mix[1].get() : nullptr,
            output.size() > 3 ? output[3].get() : nullptr
        };
        if (output.size() > 3 && output[3] != nullptr)
            output[3]->label.setText ("out", juce::dontSendNotification);
        for (auto* fader : sourceRows)
            layLine (fader, sources.removeFromTop (sourceH));
        for (int i = 2; i < 6 && i < (int) mix.size(); ++i)
            layLine (mix[(size_t) i].get(), effects.removeFromTop (effectH));
    }

    auto pinMode = [] (Knob& knob, juce::ToggleButton& tick, juce::ToggleButton* retrig = nullptr)
    {
        auto cell = knob.slider.getBounds();
        auto row = cell.removeFromBottom (18);
        cell.removeFromBottom (4);
        knob.slider.setBounds (cell);
        if (retrig != nullptr)
        {
            const int trigW = 46;
            const int syncW = juce::jmin (104, juce::jmax (48, row.getWidth() - trigW - 4));
            tick.setBounds (row.removeFromLeft (syncW).withHeight (16));
            row.removeFromLeft (juce::jmin (4, row.getWidth()));
            retrig->setBounds (row.removeFromLeft (juce::jmin (trigW, row.getWidth())).withHeight (16));
            return;
        }
        const int w = juce::jmin (104, row.getWidth());
        tick.setBounds (row.withSizeKeepingCentre (w, 16));
    };
    if (! output.empty() && output[0] != nullptr)
        pinMode (*output[0], delaySync, &delayRetrig);

    auto layLfo = [&] (juce::Rectangle<int> col, juce::Label& shapeLabel,
                       juce::ComboBox& shape, juce::Label& destLabel, juce::ComboBox& dest,
                       Knob* rate, Knob* depth, juce::ToggleButton& tick,
                       juce::ToggleButton& retrig, bool shown)
    {
        auto showPart = [shown] (juce::Component& c) { c.setVisible (shown); };
        showPart (shapeLabel);
        showPart (shape);
        showPart (destLabel);
        showPart (dest);
        showPart (tick);
        showPart (retrig);
        auto showKnob = [&] (Knob* knob)
        {
            if (knob == nullptr)
                return;
            knob->slider.setVisible (shown);
            knob->label.setVisible (shown);
        };
        showKnob (rate);
        showKnob (depth);
        placeDrop (shapeLabel, shape, col.removeFromTop (dropH));
        const int knobH = juce::jmax (96, col.getHeight() - dropH - 6);
        auto knobs = col.removeFromTop (knobH);
        auto placeOne = [] (Knob* knob, juce::Rectangle<int> cell)
        {
            if (knob == nullptr)
                return;
            knob->label.setBounds (cell.removeFromTop (16));
            knob->hint.setBounds ({});
            knob->slider.setBounds (cell);
        };
        placeOne (rate, knobs.removeFromLeft (knobs.getWidth() * 3 / 5).reduced (2, 0));
        placeOne (depth, knobs.reduced (2, 0));
        if (rate != nullptr)
            pinMode (*rate, tick, &retrig);
        placeDrop (destLabel, dest, col.removeFromTop (dropH));
    };
    auto box = lfoPanel.reduced (6, 4);
    lfoTag = box.removeFromTop (22);
    for (int i = 0; i < 4; ++i)
        lfoTab[i].setVisible (false);
    if (lfo.size() >= 8)
    {
        const int half = juce::jmax (0, (box.getWidth() - 8) / 2);
        auto lfoLeft = box.removeFromLeft (half);
        box.removeFromLeft (8);
        lfoTitle[0].setVisible (true);
        lfoTitle[1].setVisible (true);
        lfoTitle[2].setVisible (false);
        lfoTitle[3].setVisible (false);
        lfoTitle[0].setBounds (lfoLeft.removeFromTop (18));
        lfoTitle[1].setBounds (box.removeFromTop (18));
        layLfo (lfoLeft, lfoShapeLabel[0], lfoShapeBox[0], lfoDestLabel[0], lfoDestBox[0],
                lfo[0].get(), lfo[1].get(), lfoSync[0], lfoRetrig[0], true);
        layLfo (box, lfoShapeLabel[1], lfoShapeBox[1], lfoDestLabel[1], lfoDestBox[1],
                lfo[2].get(), lfo[3].get(), lfoSync[1], lfoRetrig[1], true);
        for (int i = 2; i < 4; ++i)
            layLfo ({}, lfoShapeLabel[i], lfoShapeBox[i], lfoDestLabel[i], lfoDestBox[i],
                    lfo[(size_t) (i * 2)].get(), lfo[(size_t) (i * 2 + 1)].get(), lfoSync[i], lfoRetrig[i], false);
    }
    bakeBackdrop();
}

void ClicksEditor::refreshSource()
{
}

void ClicksEditor::routeSlot (int index)
{
    if (slotGuard || index < 0 || index >= 6)
        return;
    const int chosen = slotBox[index].getSelectedItemIndex();
    if (chosen < 0)
        return;
    int missing = -1;
    bool seen[6] = {};
    for (int i = 0; i < 6; ++i)
    {
        const int s = slotBox[i].getSelectedItemIndex();
        if (s >= 0 && s < 6)
            seen[s] = true;
    }
    for (int s = 0; s < 6; ++s)
        if (! seen[s])
            missing = s;
    if (missing < 0)
        return;
    slotGuard = true;
    for (int i = 0; i < 6; ++i)
    {
        if (i == index)
            continue;
        if (slotBox[i].getSelectedItemIndex() == chosen)
            slotBox[i].setSelectedItemIndex (missing, juce::sendNotificationSync);
    }
    slotGuard = false;
}

void ClicksEditor::refreshFilter()
{
    auto fadeKnob = [] (Knob& knob, bool live)
    {
        const float fade = live ? 1.f : 0.38f;
        knob.slider.setEnabled (live);
        knob.label.setEnabled (live);
        knob.hint.setEnabled (live);
        knob.slider.setAlpha (fade);
        knob.label.setAlpha (fade);
        knob.hint.setAlpha (fade);
    };
    auto fadeAt = [&] (std::vector<std::unique_ptr<Knob>>& knobs, int index, bool live)
    {
        if (index < 0 || index >= (int) knobs.size() || knobs[(size_t) index] == nullptr)
            return;
        fadeKnob (*knobs[(size_t) index], live);
    };
    auto dress = [&] (std::vector<std::unique_ptr<Knob>>& knobs, int type, int form0, int ring0)
    {
        const bool on = type > 0;
        const bool form = type == 5;
        const bool ring = type == 6;
        fadeAt (knobs, 0, on);
        fadeAt (knobs, 1, on);
        if (! knobs.empty() && knobs[0] != nullptr)
            knobs[0]->hint.setText (form ? "vowel" : ring ? "ring" : type == 4 ? "reject" : "hertz",
                                    juce::dontSendNotification);
        if (knobs.size() > 1 && knobs[1] != nullptr && type == 4)
            knobs[1]->hint.setText ("narrow", juce::dontSendNotification);
        else if (knobs.size() > 1 && knobs[1] != nullptr)
            knobs[1]->hint.setText ("sharpness", juce::dontSendNotification);
        for (int i = 0; i < 3; ++i)
            fadeAt (knobs, form0 + i, form);
        for (int i = 0; i < 3; ++i)
            fadeAt (knobs, ring0 + i, ring);
    };
    const int type = (int) clicks.apvts.getRawParameterValue ("filttype")->load();
    const int type2 = (int) clicks.apvts.getRawParameterValue ("filt2type")->load();
    dress (filter, type, 2, 5);
    dress (filter2, type2, 3, 6);
    fadeAt (filter2, 2, true);
    const int shown = filtPage == 0 ? type : type2;
    if (shown != filtTypeShown)
    {
        filtTypeShown = shown;
        repaint (formantTag);
        repaint (ringTag);
    }
}

void ClicksEditor::refreshCrush()
{
}

ClicksEditor::Knob* ClicksEditor::knobForDest (int dest)
{
    auto at = [] (std::vector<std::unique_ptr<Knob>>& group, int index) -> Knob*
    {
        if (index < 0 || index >= (int) group.size())
            return nullptr;
        return group[(size_t) index].get();
    };
    switch (dest)
    {
        case 1: return at (filter, 0);
        case 2: return at (filter, 1);
        case 3: return at (output, 0);
        case 4: return at (output, 2);
        case 5: return at (generator, 6);
        case 6: return at (generator, 5);
        case 7: return at (generator, 2);
        case 8: return at (output, 3);
        case 9: return at (generator, 3);
        case 10: return at (generator, 4);
        case 11: return at (output, 1);
        case 12: return at (generator, 8);
        case 13: return at (generator, 0);
        case 14: return at (generator, 1);
        case 15: return at (filter, 2);
        case 16: return at (filter, 3);
        case 17: return at (filter, 4);
        case 18: return at (filter, 5);
        case 19: return at (filter, 6);
        case 20: return at (filter, 7);
        case 21: return at (env, 0);
        case 22: return at (env, 1);
        case 23: return at (env, 2);
        case 24: return at (env, 3);
        case 25: return at (fenv, 4);
        case 26: return at (sam, 0);
        case 27: return at (sam, 1);
        case 28: return at (generator, 7);
        case 29: return at (fenv, 0);
        case 30: return at (fenv, 1);
        case 31: return at (fenv, 2);
        case 32: return at (fenv, 3);
        case 33: return at (generator, 9);
        case 34: return at (generator, 10);
        case 35: return at (sam, 4);
        default: return nullptr;
    }
}

void ClicksEditor::refreshLfo()
{
    int aimed[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; ++i)
    {
        const juce::String id = "lfo" + juce::String (i + 1) + "dest";
        aimed[i] = (int) clicks.apvts.getRawParameterValue (id)->load();
    }

    Knob* marked[48] {};
    int nMarked = 0;
    auto remember = [&] (Knob* knob)
    {
        if (knob == nullptr)
            return;
        for (int i = 0; i < nMarked; ++i)
            if (marked[i] == knob)
                return;
        if (nMarked < 48)
            marked[nMarked++] = knob;
    };
    for (int dest = 1; dest < kDestN; ++dest)
        remember (knobForDest (dest));

    for (int i = 0; i < nMarked; ++i)
    {
        auto* knob = marked[i];
        int tag = 0;
        for (int n = 0; n < 4; ++n)
            if (knobForDest (aimed[n]) == knob)
                tag |= 1 << n;
        const int previous = (int) knob->slider.getProperties().getWithDefault ("lfo", 0);
        knob->slider.getProperties().set ("lfo", tag);
        juce::Colour ink = kPencil;
        for (int n = 0; n < 4; ++n)
            if ((tag & (1 << n)) != 0)
                ink = kLfoCol[n];
        knob->label.setColour (juce::Label::textColourId, ink);
        knob->slider.setColour (juce::Slider::textBoxTextColourId, ink);
        if (previous != tag)
            knob->slider.repaint();
    }
}

void ClicksEditor::snapDivision (Knob& knob, const char* id, const char* syncId,
                                 TempoHold& hold, const juce::String& freeHint)
{
    auto* param = clicks.apvts.getParameter (id);
    if (param == nullptr)
        return;
    const bool synced = clicks.apvts.getRawParameterValue (syncId)->load() > 0.5f;
    const float raw = clicks.apvts.getRawParameterValue (id)->load();
    if (synced)
    {
        if (! hold.on)
        {
            hold.saved = raw;
            hold.on = true;
            knob.slider.textFromValueFunction = [param] (double value)
            {
                const int index = ClickParams::divisionIndexNorm (param->convertTo0to1 ((float) value));
                return juce::String (ClickParams::divisionName (index));
            };
            knob.slider.valueFromTextFunction = [param] (const juce::String& text)
            {
                for (int i = 0; i < ClickParams::divisionCount(); ++i)
                    if (text.equalsIgnoreCase (ClickParams::divisionName (i)))
                        return (double) param->convertFrom0to1 ((float) i / (float) (ClickParams::divisionCount() - 1));
                return text.getDoubleValue();
            };
        }
        const int index = ClickParams::divisionIndexNorm (param->getValue());
        const float anchor = param->convertFrom0to1 ((float) index / (float) (ClickParams::divisionCount() - 1));
        knob.hint.setText (ClickParams::divisionName (index), juce::dontSendNotification);
        if (std::abs (raw - anchor) > std::max (0.0008f, std::abs (anchor) * 0.002f))
            param->setValueNotifyingHost (param->convertTo0to1 (anchor));
        knob.slider.updateText();
    }
    else if (hold.on)
    {
        hold.on = false;
        knob.slider.textFromValueFunction = nullptr;
        knob.slider.valueFromTextFunction = nullptr;
        param->setValueNotifyingHost (param->convertTo0to1 (hold.saved));
        knob.slider.updateText();
        knob.hint.setText (freeHint, juce::dontSendNotification);
    }
}

void ClicksEditor::refreshScatter()
{
    const bool repeat = clicks.apvts.getRawParameterValue ("scatmode")->load() > 0.5f;
    if (generator.size() > 9 && generator[9] != nullptr)
    {
        auto& knob = *generator[9];
        const float fade = repeat ? 1.f : 0.38f;
        knob.slider.setEnabled (repeat);
        knob.label.setEnabled (repeat);
        knob.hint.setEnabled (repeat);
        knob.slider.setAlpha (fade);
        knob.label.setAlpha (fade);
        knob.hint.setAlpha (fade);
    }
}

void ClicksEditor::refreshSam()
{
    const int wave = (int) std::lround (clicks.apvts.getRawParameterValue ("wave")->load());
    const bool samMode = wave == 6;
    const bool speakOn = wave == 7;
    const bool on = samMode || speakOn;
    const float fade = on ? 1.f : 0.38f;
    auto dim = [on, fade] (juce::Component& c)
    {
        c.setVisible (true);
        c.setEnabled (on);
        c.setAlpha (fade);
    };
    auto show = [] (juce::Component& c, bool visible, bool enabled)
    {
        c.setVisible (visible);
        c.setEnabled (enabled && visible);
        c.setAlpha (visible ? 1.f : 0.f);
    };
    dim (sayLabel);
    dim (sayBox);
    dim (lockButton);
    for (auto& knob : this->sam)
        if (knob != nullptr)
        {
            show (knob->slider, ! speakOn, samMode);
            show (knob->label, ! speakOn, samMode);
            show (knob->hint, false, false);
        }
    for (auto& knob : this->speak)
        if (knob != nullptr)
        {
            show (knob->slider, speakOn, speakOn);
            show (knob->label, speakOn, speakOn);
            show (knob->hint, false, false);
        }
    if (wave != waveShown)
    {
        waveShown = wave;
        repaint (samTag);
    }
}

void ClicksEditor::timerCallback()
{
    juce::String learnedParam;
    const int learnedCc = clicks.takeMidiLearnNotice (learnedParam);
    if (learnedCc >= 0)
    {
        learnUntil = 0;
        showNote ("MIDI CC " + juce::String (learnedCc + 1) + " controls "
                  + clicks.paramNameForId (learnedParam) + ".");
    }
    if (learnUntil > 0 && clicks.isMidiLearning()
        && juce::Time::getMillisecondCounter() > learnUntil)
    {
        clicks.cancelMidiLearn();
        learnUntil = 0;
        showNote ("MIDI learn timed out.");
    }
    if (status.isNotEmpty() && juce::Time::getMillisecondCounter() > statusUntil)
    {
        status.clear();
        showPreset();
        repaint();
    }
    // A host program change lands here too.
    const auto named = clicks.apvts.state.getProperty ("preset").toString();
    if (named != presetName)
    {
        presetName = named;
        showPreset();
    }
    refreshSam();
    refreshFilter();
    refreshSource();
    refreshCrush();
    refreshScatter();
    if (generator.size() > 2 && generator[2] != nullptr)
    {
        auto& slow = generator[2]->slider;
        const bool synced = clicks.apvts.getRawParameterValue ("slowsync")->load() > 0.5f;
        const float raw = clicks.apvts.getRawParameterValue ("slow")->load();
        if (synced)
        {
            if (! tempoOn)
            {
                freeSlow = raw;
                tempoOn = true;
                slow.textFromValueFunction = [] (double value)
                {
                    return juce::String (ClickParams::divisionName (ClickParams::divisionIndex ((float) value)));
                };
                slow.valueFromTextFunction = [] (const juce::String& text)
                {
                    for (int i = 0; i < ClickParams::divisionCount(); ++i)
                        if (text.equalsIgnoreCase (ClickParams::divisionName (i)))
                            return (double) ClickParams::divisionAnchor (i);
                    return text.getDoubleValue();
                };
            }
            const int index = ClickParams::divisionIndex (raw);
            const float anchor = ClickParams::divisionAnchor (index);
            generator[2]->hint.setText (ClickParams::divisionName (index), juce::dontSendNotification);
            if (std::abs (raw - anchor) > 0.0008f)
                if (auto* param = clicks.apvts.getParameter ("slow"))
                    param->setValueNotifyingHost (param->convertTo0to1 (anchor));
            slow.updateText();
        }
        else if (tempoOn)
        {
            tempoOn = false;
            slow.textFromValueFunction = nullptr;
            slow.valueFromTextFunction = nullptr;
            if (auto* param = clicks.apvts.getParameter ("slow"))
                param->setValueNotifyingHost (param->convertTo0to1 (freeSlow));
            slow.updateText();
            generator[2]->hint.setText ("rate", juce::dontSendNotification);
        }
    }
    if (env.size() > 3 && env[0] != nullptr && env[1] != nullptr && env[3] != nullptr)
    {
        snapDivision (*env[0], "attack", "attacksync", attackHold, {});
        snapDivision (*env[1], "decay", "decaysync", decayHold, {});
        snapDivision (*env[3], "release", "releasesync", releaseHold, {});
    if (senv.size() > 3 && senv[0] != nullptr && senv[1] != nullptr && senv[3] != nullptr)
    {
        snapDivision (*senv[0], "subattack", "subattacksync", subAttackHold, {});
        snapDivision (*senv[1], "subdecay", "subdecaysync", subDecayHold, {});
        snapDivision (*senv[3], "subrelease", "subreleasesync", subReleaseHold, {});
    }
    }
    if (! output.empty() && output[0] != nullptr)
        snapDivision (*output[0], "delay", "delaysync", delayHold, "echo time");
    if (fenv.size() > 3 && fenv[0] != nullptr && fenv[1] != nullptr && fenv[3] != nullptr)
    {
        snapDivision (*fenv[0], "fattack", "fattacksync", fAttackHold, {});
        snapDivision (*fenv[1], "fdecay", "fdecaysync", fDecayHold, {});
        snapDivision (*fenv[3], "frelease", "freleasesync", fReleaseHold, {});
    if (fenv2.size() > 3 && fenv2[0] != nullptr && fenv2[1] != nullptr && fenv2[3] != nullptr)
    {
        snapDivision (*fenv2[0], "f2attack", "f2attacksync", f2AttackHold, {});
        snapDivision (*fenv2[1], "f2decay", "f2decaysync", f2DecayHold, {});
        snapDivision (*fenv2[3], "f2release", "f2releasesync", f2ReleaseHold, {});
    }
    }
    if (lfo.size() >= 4 && lfo[0] != nullptr && lfo[2] != nullptr)
    {
        for (int i = 0; i < 4; ++i)
            if (lfo.size() > (size_t) (i * 2) && lfo[(size_t) (i * 2)] != nullptr)
                snapDivision (*lfo[(size_t) (i * 2)],
                              ("lfo" + juce::String (i + 1) + "rate").toRawUTF8(),
                              ("lfo" + juce::String (i + 1) + "sync").toRawUTF8(),
                              lfoHold[i], "hz");
    }
    refreshLfo();
    if (clicks.takeSpeechRequest())
        clicks.lockSpeech (false);
    if (clicks.samNeedsRender())
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (lastSamLive == 0 || now - lastSamLive >= 140)
        {
            lastSamLive = now;
            const auto speechStatus = clicks.lockSpeech (false);
            if (speechStatus.startsWith ("That line") || speechStatus.startsWith ("Sam could"))
                showNote (speechStatus);
        }
    }
    const int rev = clicks.samRevision();
    if (rev != samSeen)
    {
        samSeen = rev;
        const auto word = clicks.samText();
        if (sayBox.getText() != word)
            sayBox.setText (word, juce::dontSendNotification);
    }
    refreshScope();
    const float peak = clicks.meter.load();
    if (peak >= meterShown)
        meterShown = peak;
    else
        meterShown *= 0.86f;
    if (peak >= meterHold)
    {
        meterHold = peak;
        meterHoldAge = 0;
    }
    else if (++meterHoldAge > 18)
        meterHold *= 0.9f;
    repaint (scopePanel);
    repaint (wavePlate);
    repaint (subWavePlate);
    repaint (botPanel);
    repaint (outMeter.getBounds());
    animateLfos();
}

void ClicksEditor::animateLfos()
{
    Knob* knobs[48] {};
    float next[48] {};
    int n = 0;
    auto find = [&] (Knob* knob) -> int
    {
        if (knob == nullptr)
            return -1;
        for (int i = 0; i < n; ++i)
            if (knobs[i] == knob)
                return i;
        if (n >= 48)
            return -1;
        knobs[n] = knob;
        return n++;
    };
    for (int dest = 1; dest < kDestN; ++dest)
        find (knobForDest (dest));
    const bool speedFree = clicks.apvts.getRawParameterValue ("slowsync")->load() < 0.5f;
    for (int i = 0; i < 4; ++i)
    {
        const int dest = (int) clicks.apvts.getRawParameterValue ("lfo" + juce::String (i + 1) + "dest")->load();
        if (dest <= 0 || (dest == 7 && ! speedFree))
            continue;
        const int slot = find (knobForDest (dest));
        if (slot >= 0)
            next[slot] += clicks.lfoAmount (i);
    }
    for (int i = 0; i < n; ++i)
    {
        auto& props = knobs[i]->slider.getProperties();
        const float prev = (float) props.getWithDefault ("lfoDrive", 0.0);
        if (std::abs (prev - next[i]) < 0.004f)
            continue;
        props.set ("lfoDrive", next[i]);
        repaint (knobs[i]->slider.getBoundsInParent().expanded (8));
    }
}

juce::File ClicksEditor::presetFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                   .getChildFile ("Library/Audio/Presets/Logic Beach/NULL GLITCH");
    dir.createDirectory();
    return dir;
}

void ClicksEditor::initPreset()
{
    choosePreset (0);
}

void ClicksEditor::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Save preset",
        presetFolder().getChildFile ("preset.nullglitch"),
        "*.nullglitch",
        true, false, this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                               | juce::FileBrowserComponent::canSelectFiles
                               | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& dialog)
                          {
                              auto file = dialog.getResult();
                              if (file == juce::File())
                                  return;
                              if (! file.hasFileExtension ("nullglitch"))
                                  file = file.withFileExtension ("nullglitch");
                              presetName = file.getFileNameWithoutExtension();
                              clicks.apvts.state.setProperty ("preset", presetName, nullptr);
                              if (auto xml = clicks.apvts.copyState().createXml())
                              {
                                  if (xml->writeTo (file))
                                      showNote ("Saved " + file.getFileName());
                                  else
                                      showPreset();
                              }
                          });
}

void ClicksEditor::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load preset",
        presetFolder(),
        "*.nullglitch",
        true, false, this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& dialog)
                          {
                              const auto file = dialog.getResult();
                              if (file == juce::File())
                                  return;
                              const auto xml = juce::XmlDocument::parse (file);
                              if (xml == nullptr || ! xml->hasTagName (clicks.apvts.state.getType()))
                              {
                                  showNote ("That file is not a NULL GLITCH preset.");
                                  return;
                              }
                              clicks.replacePluginState (*xml);
                              presetName = clicks.apvts.state.getProperty ("preset").toString();
                              if (presetName.isEmpty())
                                  presetName = file.getFileNameWithoutExtension();
                              clicks.apvts.state.setProperty ("preset", presetName, nullptr);
                              clicks.lockSpeech (false);
                              showNote ("Loaded " + file.getFileName());
                          });
}

void ClicksEditor::refreshScope()
{
    auto fill = [this] (std::vector<float>& trace, int width, double& period, int n, const float* raw)
    {
        trace.assign ((size_t) std::max (2, width), 0.f);
        if (n < 64)
        {
            period = 0;
            return;
        }
        const auto locked = lockBassScope (raw, n, clicks.engineSampleRate(),
                                           (double) clicks.bassHz(), period);
        period = locked.period;
        if (locked.length < 2)
            return;
        const float* w = raw + locked.start;
        float peak = 0.f;
        for (int i = 0; i < locked.length; ++i)
            peak = std::max (peak, std::abs (w[i]));
        const float scale = peak < 0.008f ? 0.f : std::min (1.f / peak, 6.f);
        const int plot = std::max (2, width);
        for (int i = 0; i < plot; ++i)
        {
            const double pos = (double) i / (double) (plot - 1) * (double) (locked.length - 1);
            const int a = (int) pos;
            const int b = std::min (a + 1, locked.length - 1);
            const float f = (float) (pos - a);
            trace[(size_t) i] = (w[a] * (1.f - f) + w[b] * f) * scale;
        }
    };

    float raw[8192];
    if (! scopePanel.isEmpty())
    {
        const int n = clicks.copyScope (raw, 8192);
        fill (scopeTrace, std::max (2, scopePanel.getWidth() - 4), scopePeriod, n, raw);
    }
    if (! botPanel.isEmpty())
    {
        const int plot = std::max (2, botPanel.getWidth() - 4);
        if ((int) std::lround (clicks.apvts.getRawParameterValue ("wave")->load()) < 6)
        {
            samTrace.assign ((size_t) plot, 0.f);
            samPeriod = 0;
        }
        else
        {
            const int n = clicks.copySamScope (raw, 8192);
            fill (samTrace, plot, samPeriod, n, raw);
        }
    }
}

void ClicksEditor::showPreset()
{
    presetBox.setText (status.isNotEmpty() ? status : presetName, juce::dontSendNotification);
}

void ClicksEditor::showNote (const juce::String& text)
{
    status = text;
    statusUntil = juce::Time::getMillisecondCounter() + 2800;
    showPreset();
    repaint();
}

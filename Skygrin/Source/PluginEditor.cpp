#include "PluginEditor.h"

using namespace SkygrinColours;

//==============================================================================
SkygrinLookAndFeel::SkygrinLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId,            background);
    setColour (juce::PopupMenu::textColourId,                  ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ink.withAlpha (0.10f));
    setColour (juce::PopupMenu::highlightedTextColourId,       ink);
    setColour (juce::ComboBox::textColourId,                   ink);
}

void SkygrinLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float thickness = radius * 0.13f;
    const float arcRadius = radius - thickness * 0.5f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         startAngle, endAngle, true);
    g.setColour (ink.withAlpha (0.10f));
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // disque central
    const float innerR = radius - thickness * 1.9f;
    g.setColour (background);
    g.fillEllipse (centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f);
    g.setColour (ink.withAlpha (0.85f));
    g.drawEllipse (centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f, 1.6f);

    // repere
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.8f, -innerR + 6.0f, 3.6f, innerR * 0.42f, 1.8f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (ink);
    g.fillPath (pointer);
}

void SkygrinLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.8f);

    g.setColour (background);
    g.fillRoundedRectangle (r, height * 0.5f);
    g.setColour (ink.withAlpha (0.55f));
    g.drawRoundedRectangle (r, height * 0.5f, 1.3f);

    // petite fleche
    juce::Path arrow;
    const float cx = r.getRight() - 18.0f;
    const float cy = r.getCentreY();
    arrow.startNewSubPath (cx - 4.5f, cy - 2.0f);
    arrow.lineTo (cx, cy + 3.0f);
    arrow.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (ink.withAlpha (0.8f));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

juce::Font SkygrinLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (15.0f));
}

void SkygrinLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (14, 1, box.getWidth() - 36, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

//==============================================================================
void SmileyComponent::paint (juce::Graphics& g)
{
    const float t = juce::jlimit (0.0f, 1.0f, value);
    auto area = getLocalBounds().toFloat();

    const float R = juce::jmin (area.getWidth(), area.getHeight()) * 0.34f;
    const float shake = t * t * t * 3.2f;

    juce::Point<float> c (area.getCentreX() + std::sin (phase * 37.0f) * shake,
                          area.getCentreY() - R * 0.12f * t + std::sin (phase * 53.0f) * shake * 0.6f);

    const auto accent = accentFor (t);

    // ---- rayons ------------------------------------------------------------
    if (t > 0.5f)
    {
        const float amount = (t - 0.5f) * 2.0f;
        const float len = R * 0.10f + amount * R * 0.26f;
        g.setColour (accent.darker (0.25f).withAlpha (0.35f + 0.55f * amount));

        for (int i = 0; i < 12; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 12.0f + phase * 0.7f;
            const float x1 = c.x + std::cos (a) * (R * 1.16f);
            const float y1 = c.y + std::sin (a) * (R * 1.16f);
            const float x2 = c.x + std::cos (a) * (R * 1.16f + len);
            const float y2 = c.y + std::sin (a) * (R * 1.16f + len);
            g.drawLine (x1, y1, x2, y2, 2.4f);
        }
    }

    // ---- tete --------------------------------------------------------------
    g.setColour (accent);
    g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    g.setColour (ink);
    g.drawEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f, 2.6f);

    // ---- sourcils ----------------------------------------------------------
    {
        const float browY = c.y - R * 0.52f - t * R * 0.10f;
        const float halfLen = R * 0.22f;
        const float tilt = 0.42f - t * 0.95f;      // + = triste, - = surpris

        for (int side = 0; side < 2; ++side)
        {
            const float sign = (side == 0) ? -1.0f : 1.0f;
            const float bx = c.x + sign * R * 0.40f;
            const float dy = std::sin (tilt) * halfLen * sign * -1.0f;

            juce::Path brow;
            brow.startNewSubPath (bx - halfLen, browY - dy);
            brow.lineTo (bx + halfLen, browY + dy);
            g.setColour (ink);
            g.strokePath (brow, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
    }

    // ---- yeux --------------------------------------------------------------
    {
        const float eyeY = c.y - R * 0.20f;
        const float eyeR = R * 0.105f * (1.0f + t * 0.55f);

        for (int side = 0; side < 2; ++side)
        {
            const float sign = (side == 0) ? -1.0f : 1.0f;
            const float ex = c.x + sign * R * 0.40f;
            g.setColour (ink);
            g.fillEllipse (ex - eyeR, eyeY - eyeR, eyeR * 2.0f, eyeR * 2.0f);
            g.setColour (background.withAlpha (0.9f));
            g.fillEllipse (ex - eyeR * 0.30f, eyeY - eyeR * 0.60f, eyeR * 0.45f, eyeR * 0.45f);
        }
    }

    // ---- bouche ------------------------------------------------------------
    {
        const float mouthY = c.y + R * 0.26f;
        const float halfW  = R * 0.38f + t * R * 0.20f;
        const float curve  = -R * 0.26f + t * R * 1.15f;

        juce::Path mouth;
        mouth.startNewSubPath (c.x - halfW, mouthY);
        mouth.quadraticTo (c.x, mouthY + curve, c.x + halfW, mouthY);

        if (t > 0.42f)
        {
            mouth.closeSubPath();
            g.setColour (ink);
            g.fillPath (mouth);

            if (t > 0.6f)   // dents
            {
                const float th = juce::jmap (t, 0.6f, 1.0f, 2.0f, R * 0.13f);
                g.setColour (background);
                g.fillRoundedRectangle (c.x - halfW * 0.82f, mouthY + 1.5f,
                                        halfW * 1.64f, th, 2.0f);
            }
        }
        else
        {
            g.setColour (ink);
            g.strokePath (mouth, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }
    }
}

//==============================================================================
SkygrinAudioProcessorEditor::SkygrinAudioProcessorEditor (SkygrinAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (smiley);

    intensityKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    intensityKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    intensityKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                       juce::MathConstants<float>::pi * 2.75f, true);
    intensityKnob.setColour (juce::Slider::rotarySliderFillColourId, cold);
    addAndMakeVisible (intensityKnob);

    readout.setJustificationType (juce::Justification::centred);
    readout.setColour (juce::Label::textColourId, ink);
    readout.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
    readout.setText ("0 %", juce::dontSendNotification);
    addAndMakeVisible (readout);

    presetBox.addItemList (getPresetNames(), 1);
    addAndMakeVisible (presetBox);

    knobAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "intensity", intensityKnob);
    presetAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.apvts, "preset", presetBox);

    setSize (400, 520);
    startTimerHz (45);
}

SkygrinAudioProcessorEditor::~SkygrinAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SkygrinAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")).withExtraKerningFactor (0.34f));
    g.drawText ("SKYGRIN", 0, 20, getWidth(), 24, juce::Justification::centred);

    g.setColour (ink.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    g.drawText ("AOCIZ  \xe2\x80\xa2  BUILD-UP MACHINE", 0, 46, getWidth(), 14,
                juce::Justification::centred);

    g.setColour (ink.withAlpha (0.12f));
    g.drawLine (40.0f, 470.0f, (float) getWidth() - 40.0f, 470.0f, 1.0f);
}

void SkygrinAudioProcessorEditor::resized()
{
    smiley.setBounds (0, 66, getWidth(), 190);
    intensityKnob.setBounds (getWidth() / 2 - 88, 258, 176, 176);
    readout.setBounds (0, 436, getWidth(), 28);
    presetBox.setBounds (getWidth() / 2 - 105, 482, 210, 30);
}

void SkygrinAudioProcessorEditor::timerCallback()
{
    const float actual = processorRef.uiIntensity.load();
    displayed += (actual - displayed) * 0.22f;
    phase += 0.016f;

    smiley.setValues (displayed, phase);
    smiley.repaint();

    const auto accent = accentFor (displayed);
    if (intensityKnob.findColour (juce::Slider::rotarySliderFillColourId) != accent)
    {
        intensityKnob.setColour (juce::Slider::rotarySliderFillColourId, accent);
        intensityKnob.repaint();
    }

    const juce::String txt = juce::String (juce::roundToInt (actual * 100.0f)) + " %";
    if (readout.getText() != txt)
        readout.setText (txt, juce::dontSendNotification);
}

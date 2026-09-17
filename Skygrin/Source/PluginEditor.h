#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
namespace SkygrinColours
{
    const juce::Colour background { 0xfff6f3ec };
    const juce::Colour ink        { 0xff1b1a1e };
    const juce::Colour cold       { 0xffbfd8ee };   // pastel bleu
    const juce::Colour mid        { 0xfff6de9b };   // pastel jaune
    const juce::Colour hot        { 0xfff07d23 };   // orange

    inline juce::Colour accentFor (float t) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return (t < 0.5f) ? cold.interpolatedWith (mid, t * 2.0f)
                          : mid.interpolatedWith (hot, (t - 0.5f) * 2.0f);
    }
}

//==============================================================================
class SkygrinLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkygrinLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
//  Le petit bonhomme : il passe de la moue au sourire maximum.
//==============================================================================
class SmileyComponent : public juce::Component
{
public:
    void setValues (float intensity, float animPhase) noexcept
    {
        value = intensity;
        phase = animPhase;
    }

    void paint (juce::Graphics&) override;

private:
    float value = 0.0f;
    float phase = 0.0f;
};

//==============================================================================
class SkygrinAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit SkygrinAudioProcessorEditor (SkygrinAudioProcessor&);
    ~SkygrinAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SkygrinAudioProcessor& processorRef;
    SkygrinLookAndFeel     lnf;

    SmileyComponent smiley;
    juce::Slider    intensityKnob;
    juce::Label     readout;
    juce::ComboBox  presetBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   knobAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttach;

    float displayed = 0.0f;
    float phase     = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkygrinAudioProcessorEditor)
};

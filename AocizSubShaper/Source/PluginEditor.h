#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

// ----------------------------------------------------------------------------
//  Palette « synthé modulaire »
// ----------------------------------------------------------------------------
namespace palette
{
    const juce::Colour panelTop    { 0xff26272e };
    const juce::Colour panelBottom { 0xff1b1c21 };
    const juce::Colour screen      { 0xff0b0c10 };
    const juce::Colour edge        { 0xff3a3c45 };
    const juce::Colour grid        { 0xff1e2028 };
    const juce::Colour textDim     { 0xff8d909a };
    const juce::Colour text        { 0xffeef0f4 };

    const juce::Colour red    { 0xffff4d4d };
    const juce::Colour violet { 0xffb06cff };
    const juce::Colour blue   { 0xff3d9bff };
    const juce::Colour green  { 0xff2ee59d };
    const juce::Colour orange { 0xffff9f1c };
    const juce::Colour cyan   { 0xff22d3ee };
    const juce::Colour pink   { 0xffff4d8d };
    const juce::Colour amber  { 0xffffc21a };
    const juce::Colour lime   { 0xffa3e635 };
    const juce::Colour indigo { 0xff7c6cff };

    inline juce::Colour modeColour (int mode)
    {
        static const juce::Colour c[] { red, violet, blue, green, orange };
        return c[juce::jlimit (0, 4, mode)];
    }
}

void setAccent (juce::Component&, juce::Colour);
juce::Colour getAccent (const juce::Component&, juce::Colour fallback = palette::cyan);

// ----------------------------------------------------------------------------
class ModularLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModularLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// ----------------------------------------------------------------------------
class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView (SubShaperProcessor&);
    ~SpectrumView() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float xForFreq (float hz) const;

    SubShaperProcessor& proc;
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> ring {};
    std::array<float, fftSize * 2> work {};
    std::array<float, fftSize / 2> smoothed {};
    std::vector<float> scratch;
};

// ----------------------------------------------------------------------------
struct LabelledKnob : public juce::Component
{
    LabelledKnob();
    void resized() override;
    void setColour (juce::Colour c);
    juce::Slider slider;
    juce::Label label;
};

// ----------------------------------------------------------------------------
class SubShaperEditor : public juce::AudioProcessorEditor
{
public:
    explicit SubShaperEditor (SubShaperProcessor&);
    ~SubShaperEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateModeUI (int mode);
    void drawRail (juce::Graphics&, juce::Rectangle<float>);
    void drawJack (juce::Graphics&, juce::Point<float>, const juce::String&);

    SubShaperProcessor& proc;
    ModularLookAndFeel lnf;
    SpectrumView spectrum;

    juce::OwnedArray<juce::TextButton> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    int currentMode = 0;

    LabelledKnob crossoverKnob, amountKnob, characterKnob, mixKnob, outputKnob;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAtt> crossoverAtt, amountAtt, characterAtt, mixAtt, outputAtt;

    juce::ToggleButton monoLowBtn, soloLowBtn, subCutBtn;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAtt> monoLowAtt, soloLowAtt, subCutAtt;

    juce::Label modeHint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubShaperEditor)
};

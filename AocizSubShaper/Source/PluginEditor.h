#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

// ----------------------------------------------------------------------------
//  Look & feel monochrome Aociz
// ----------------------------------------------------------------------------
class AocizLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AocizLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// ----------------------------------------------------------------------------
//  Analyseur de spectre
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
//  Potard + libellé
// ----------------------------------------------------------------------------
struct LabelledKnob : public juce::Component
{
    LabelledKnob();
    void resized() override;
    juce::Slider slider;
    juce::Label label;
};

// ----------------------------------------------------------------------------
//  Éditeur
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

    SubShaperProcessor& proc;
    AocizLookAndFeel lnf;
    SpectrumView spectrum;

    juce::OwnedArray<juce::TextButton> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    LabelledKnob crossoverKnob, amountKnob, characterKnob, mixKnob, outputKnob;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAtt> crossoverAtt, amountAtt, characterAtt, mixAtt, outputAtt;

    juce::ToggleButton monoLowBtn, soloLowBtn, subCutBtn;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAtt> monoLowAtt, soloLowAtt, subCutAtt;

    juce::Label modeHint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubShaperEditor)
};

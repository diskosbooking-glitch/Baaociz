#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include "PluginProcessor.h"

// ----------------------------------------------------------------------------
//  Palette pastel
// ----------------------------------------------------------------------------
namespace palette
{
    const juce::Colour panelTop    { 0xff2d2c33 };
    const juce::Colour panelBottom { 0xff232228 };
    const juce::Colour card        { 0xff1e1d23 };
    const juce::Colour screen      { 0xff141318 };
    const juce::Colour edge        { 0xff403e48 };
    const juce::Colour grid        { 0xff26252d };
    const juce::Colour textDim     { 0xff8f8c99 };
    const juce::Colour text        { 0xfff1eff5 };
    const juce::Colour ink         { 0xff1c1b21 };

    const juce::Colour sky      { 0xffa7cff2 };
    const juce::Colour lavender { 0xffc0b0f2 };
    const juce::Colour peach    { 0xffffc2a1 };
    const juce::Colour butter   { 0xfff0e0a0 };
    const juce::Colour rose     { 0xfff2abc8 };
    const juce::Colour mint     { 0xffa3e3c6 };
    const juce::Colour aqua     { 0xff9fd9df };
    const juce::Colour coral    { 0xfff5a9a0 };
}

void setAccent (juce::Component&, juce::Colour);
juce::Colour getAccent (const juce::Component&, juce::Colour fallback = palette::sky);

// ----------------------------------------------------------------------------
class ModularLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModularLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override {}
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
};

// ----------------------------------------------------------------------------
class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView (SubShaperProcessor&);
    ~SpectrumView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override { smoothed.fill (-100.0f); }

    // Mise en avant d'un module sur l'écran (survol, réglage, automation)
    void setFocus (const juce::String& id, bool hold);
    void flash (const juce::String& id);
    std::function<float()> playedFrequency;

private:
    void timerCallback() override;
    float xForFreq (float hz, float w) const;
    void drawOverlays (juce::Graphics&, float w, float h);

    juce::String focusId;
    float focusAlpha = 0.0f;
    bool focusHold = false;

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
class TunerView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit TunerView (SubShaperProcessor&);
    ~TunerView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    float getFrequency() const { return shownFreq; }

private:
    void timerCallback() override;
    float detect (double fs);

    SubShaperProcessor& proc;
    static constexpr int bufSize = 3072;
    std::array<float, bufSize> ring {};
    std::vector<float> scratch, diff;
    float freq = 0.0f, shownFreq = 0.0f;
    int holdFrames = 0;
};

// ----------------------------------------------------------------------------
class MeterView : public juce::Component, private juce::Timer
{
public:
    explicit MeterView (SubShaperProcessor& p) : proc (p) { startTimerHz (30); }
    ~MeterView() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    SubShaperProcessor& proc;
    float levelL = 0.0f, levelR = 0.0f;
    int clipHold = 0;
};

// ----------------------------------------------------------------------------
struct LabelledKnob : public juce::Component
{
    LabelledKnob();
    void resized() override;
    void setAccentColour (juce::Colour c);
    juce::Slider slider;
    juce::Label label;
};

// ----------------------------------------------------------------------------
class ChoiceSegments : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceSegments (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::Colour accent,
                    bool vertical, juce::UndoManager*);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool vertical;
};

// ----------------------------------------------------------------------------
struct ModuleUI
{
    juce::String name;
    juce::Colour colour;
    juce::Rectangle<int> bounds;
};

class Panel : public juce::Component, private juce::Timer
{
public:
    explicit Panel (SubShaperProcessor&);
    ~Panel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1180, baseH = 770;
    std::function<void (float)> onScaleChange;

private:
    struct FocusRelay : public juce::MouseListener
    {
        explicit FocusRelay (Panel& p) : panel (p) {}
        void mouseEnter (const juce::MouseEvent& e) override { update (e, true); }
        void mouseExit (const juce::MouseEvent& e) override  { update (e, false); }
        void mouseDown (const juce::MouseEvent& e) override  { update (e, true); }
        void update (const juce::MouseEvent& e, bool hold);
        Panel& panel;
        std::map<juce::Component*, juce::String> ids;
    };
    void registerFocus (juce::Component&, const juce::String& id);
    static juce::String moduleForParam (const juce::String& paramId);

    void timerCallback() override;
    void refreshPresetBox();
    void savePresetDialog();
    void stepPreset (int delta);
    void attachKnob (LabelledKnob&, const juce::String& id, const juce::String& label, juce::Colour, const juce::String& tip);
    void attachToggle (juce::ToggleButton&, const juce::String& id, const juce::String& text, juce::Colour, const juce::String& tip);
    void drawRail (juce::Graphics&, juce::Rectangle<float>);
    void drawCard (juce::Graphics&, juce::Rectangle<float>, juce::Colour, const juce::String& tab);

    SubShaperProcessor& proc;
    ModularLookAndFeel lnf;

    SpectrumView spectrum;
    TunerView tuner;
    MeterView meter;

    // Barre du haut
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" },
                     slotA { "A" }, slotB { "B" }, copyBtn { "COPY" },
                     undoBtn { "UNDO" }, redoBtn { "REDO" };
    juce::ComboBox presetBox, scaleBox;
    juce::Array<juce::File> userFiles;

    // KEY
    LabelledKnob keyKnob;
    std::unique_ptr<ChoiceSegments> octaveSeg;
    juce::Label keyReadout;

    // Modules
    juce::ToggleButton genOn, toneOn, driveOn, shapeOn, pumpOn, widthOn, keyLockBtn;
    std::unique_ptr<ChoiceSegments> genTypeSeg, driveTypeSeg, pumpRateSeg;
    LabelledKnob genLevel, genTone, toneAmt, toneQ, toneHarm, drive, color, focus, driveMix,
                 attack, sustain, pumpDepth, pumpShape, width;
    juce::Label toneFreqLabel, shapeInfo, pumpInfo, widthInfo;

    // Global
    LabelledKnob inGain, crossover, mix, output;
    juce::ToggleButton monoLow, soloLow, subCut, delta, gainMatch, hq;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::OwnedArray<SliderAtt> sliderAtts;
    juce::OwnedArray<ButtonAtt> buttonAtts;

    std::vector<ModuleUI> modules;
    FocusRelay focusRelay { *this };
    std::map<juce::String, float> lastValues;
    juce::String lastPresetName;
    int lastGenType = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panel)
};

// ----------------------------------------------------------------------------
class SubShaperEditor : public juce::AudioProcessorEditor
{
public:
    explicit SubShaperEditor (SubShaperProcessor&);
    ~SubShaperEditor() override;
    void resized() override;

private:
    void applyScale (float s);
    SubShaperProcessor& proc;
    Panel panel;
    juce::TooltipWindow tooltips { this, 500 };
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubShaperEditor)
};

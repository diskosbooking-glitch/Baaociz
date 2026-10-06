#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ----------------------------------------------------------------------------
//  Palette pastel (famille Aociz : Subshaper, Skygrin…)
// ----------------------------------------------------------------------------
namespace palette
{
    const juce::Colour panelTop    { 0xff2d2c33 };
    const juce::Colour panelBottom { 0xff232228 };
    const juce::Colour card        { 0xff1e1d23 };
    const juce::Colour screen      { 0xff141318 };
    const juce::Colour edge        { 0xff403e48 };
    const juce::Colour grid        { 0xff24232b };
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

    inline juce::Colour band (int i)
    {
        static const juce::Colour c[] = { sky, peach, mint, lavender, butter, rose, aqua, coral };
        return c[(size_t) ((i % 8 + 8) % 8)];
    }
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
//  Écran central : analyseur, courbe de réduction et éditeur de bandes
// ----------------------------------------------------------------------------
class SpectrumDisplay : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumDisplay (VeloursProcessor&);
    ~SpectrumDisplay() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void selectBand (int b) { selected = b; repaint(); }

private:
    void timerCallback() override;

    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForSens (float db) const;
    float sensForY (float y) const;
    float yForSpec (float db, float f) const;
    float yForRed (float db) const;

    juce::Point<float> nodePos (int band) const;
    int hitBand (juce::Point<float>) const;
    static bool usesGain (int type) { return type != params::lowCut && type != params::highCut; }

    juce::RangedAudioParameter* bandParam (int band, const char* what) const;
    void setBandValue (int band, const char* what, float value, bool gesture = true);
    void addBand (float freq, float gain, int type);
    void showBandMenu (int band);
    void showAddMenu (juce::Point<float>);
    void rebuildCurves();

    VeloursProcessor& proc;
    std::array<bands::Band, params::numBands> bandsNow {};
    int curveChannels = 0;
    bool curveMS = false;

    std::vector<float> colIn, colOut, colRed;     // par colonne de pixel (lissées)
    std::vector<int> colLo, colHi;                // cases FFT couvertes par chaque colonne
    std::vector<float> scratchIn, scratchOut, scratchRed;
    int mappedBins = -1;
    float mappedBinHz = 0.0f;
    int lastCounter = -1;

    std::vector<float> sensCurve[2];               // dB de sensibilité par colonne, par canal
    bool curvesDiffer = false;

    // Résonances suivies (lissées d'une image à l'autre)
    struct Track { float hz = 1000.0f, db = 0.0f, life = 0.0f; };
    std::array<Track, 8> tracks {};
    void updateTracks (const float* hz, const float* db, int n);
    void drawTracking (juce::Graphics&);

    // Traînées : les dernières courbes de réduction, pour voir le suivi bouger
    static constexpr int numTrails = 8;
    std::array<std::vector<float>, numTrails> trails;
    int trailPos = 0, trailTick = 0;
    float maxCutDb = 30.0f;

    int selected = -1, dragging = -1, hover = -1;
    juce::Point<float> hoverPos;
    bool showHover = false;

    juce::Rectangle<float> plot;
};

// ----------------------------------------------------------------------------
class LabelledKnob : public juce::Component
{
public:
    LabelledKnob();
    void attach (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& text,
                 juce::Colour c, const juce::String& tip, bool big = false);
    void resized() override;

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ChoiceSegments : public juce::Component
{
public:
    ChoiceSegments (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::Colour accent);
    void resized() override;
    void setTip (const juce::String& t) { for (auto* b : buttons) b->setTooltip (t); }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

class ReductionMeter : public juce::Component, private juce::Timer
{
public:
    explicit ReductionMeter (VeloursProcessor& p) : proc (p) { startTimerHz (30); }
    ~ReductionMeter() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    VeloursProcessor& proc;
    float overall = 0.0f, peak = 0.0f, peakHold = 0.0f;
    int holdFrames = 0;
};

// ----------------------------------------------------------------------------
class Panel : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseW = 1100;
    static constexpr int baseH = 720;

    explicit Panel (VeloursProcessor&);
    ~Panel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (float)> onScaleChange;

private:
    void timerCallback() override;
    void refreshPresetBox();
    void stepPreset (int step);
    void attachToggle (juce::ToggleButton&, const juce::String& id, const juce::String& text, juce::Colour, const juce::String& tip);
    void drawRail (juce::Graphics&, juce::Rectangle<float>);
    void drawCard (juce::Graphics&, juce::Rectangle<float>, juce::Colour, const juce::String& tab);

    VeloursProcessor& proc;
    ModularLookAndFeel lnf;

    SpectrumDisplay display;

    LabelledKnob depth, detail, detailTilt, attack, release, timeTilt, releaseTilt, maxCut, link, mix, wetTrim, output;
    std::unique_ptr<ChoiceSegments> modeSeg, stereoSeg;
    juce::ToggleButton sidechain, delta, bypass, bandListen;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    ReductionMeter meter;
    juce::Label scInfo;

    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::ComboBox presetBox, qualityBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
    juce::String shownPreset;

    struct Card { juce::Rectangle<int> bounds; juce::Colour colour; juce::String tab; };
    std::vector<Card> cards;
};

// ----------------------------------------------------------------------------
class VeloursEditor : public juce::AudioProcessorEditor
{
public:
    explicit VeloursEditor (VeloursProcessor&);
    ~VeloursEditor() override;
    void resized() override;

private:
    void applyScale (float);
    VeloursProcessor& proc;
    Panel panel;
    juce::TooltipWindow tooltips { this, 600 };
    float scale = 1.0f;
};

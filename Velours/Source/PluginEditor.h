#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ----------------------------------------------------------------------------
//  Velours v0.3 — interface sobre : gris profond, un seul accent « velours »,
//  typographie Inter, grand graphe de réduction au centre.
// ----------------------------------------------------------------------------
namespace ui
{
    const juce::Colour window   { 0xff16171a };
    const juce::Colour panel    { 0xff1d1e22 };
    const juce::Colour raised   { 0xff26272c };
    const juce::Colour raisedHi { 0xff2e2f35 };
    const juce::Colour line     { 0xff313238 };
    const juce::Colour graphBg  { 0xff111215 };
    const juce::Colour grid     { 0xff1f2024 };
    const juce::Colour text     { 0xffecebe8 };
    const juce::Colour dim      { 0xff8e8d94 };
    const juce::Colour faint    { 0xff5b5b62 };
    const juce::Colour accent   { 0xffe8687e };   // velours (rose profond)
    const juce::Colour ink      { 0xff141518 };

    juce::Font regular (float size);
    juce::Font semi (float size);
    juce::Font caps (float size);   // semi-gras, espacé (titres)
}

// ----------------------------------------------------------------------------
class VeloursLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VeloursLookAndFeel();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;

private:
    juce::Typeface::Ptr interRegular, interSemi;
};

// ----------------------------------------------------------------------------
//  Graphe de réduction + courbe de profondeur (éditeur de bandes)
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

    int getSelected() const { return selected; }
    void selectBand (int b) { selected = b; if (onSelectionChange) onSelectionChange (b); repaint(); }
    std::function<void (int)> onSelectionChange;

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
    static bool usesGain (int type) { return type != params::lowCut && type != params::highCut && type != params::bandReject; }

    juce::RangedAudioParameter* bandParam (int band, const char* what) const;
    void setBandValue (int band, const char* what, float value, bool gesture = true);
    void addBand (float freq, float gain, int type);
    void showBandMenu (int band);
    void showAddMenu (juce::Point<float>);
    void rebuildCurves();
    void updateTracks (const float* hz, const float* db, int n);
    void drawTracking (juce::Graphics&);

    VeloursProcessor& proc;
    std::array<bands::Band, params::numBands> bandsNow {};
    int curveChannels = 0;
    bool curveMS = false;

    std::vector<float> colIn, colRed, colHold;    // par colonne de pixel (lissées)
    std::vector<int> colLo, colHi;
    std::vector<float> scratchIn, scratchRed;
    int mappedBins = -1;
    float mappedBinHz = 0.0f;
    int lastCounter = -1;

    std::vector<float> sensCurve[2];
    bool curvesDiffer = false;

    struct Track { float hz = 1000.0f, db = 0.0f, life = 0.0f; };
    std::array<Track, 8> tracks {};

    float maxCutDb = 30.0f;
    int selected = -1, dragging = -1, hover = -1;
    juce::Point<float> hoverPos;
    bool showHover = false;
    juce::Rectangle<float> plot;
};

// ----------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    enum class Size { big, medium, small };
    Knob();
    void attach (VeloursProcessor&, const juce::String& id, const juce::String& text, Size, const juce::String& tip,
                 bool useAccent = false);
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;
private:
    juce::String title;
    Size size = Size::medium;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class HSlider : public juce::Component
{
public:
    HSlider();
    void attach (VeloursProcessor&, const juce::String& id, const juce::String& text, const juce::String& tip);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Segments : public juce::Component
{
public:
    Segments (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::UndoManager*);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setTip (const juce::String& t) { for (auto* b : buttons) b->setTooltip (t); }
private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

class LevelReadout : public juce::Component, private juce::Timer
{
public:
    explicit LevelReadout (VeloursProcessor& p) : proc (p) { startTimerHz (20); }
    ~LevelReadout() override { stopTimer(); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    VeloursProcessor& proc;
    float overall = 0.0f, peak = 0.0f;
};

// ----------------------------------------------------------------------------
//  Barre de la bande sélectionnée (comme soothe3) : on/off, écoute, forme,
//  fréquence, profondeur, Q, focus, suppression
// ----------------------------------------------------------------------------
class BandStrip : public juce::Component
{
public:
    explicit BandStrip (VeloursProcessor&);
    ~BandStrip() override;
    void setBand (int band);
    std::function<void()> onDelete;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    VeloursProcessor& proc;
    int band = -1;
    juce::TextButton power { "ON" }, listen { "LISTEN" }, remove { "X" };
    juce::ComboBox shape, focus;
    juce::Slider freq, gain, q;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAtt, focusAtt;
    std::unique_ptr<juce::ParameterAttachment> bypAtt;
};

// ----------------------------------------------------------------------------
class Panel : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseW = 1100;
    static constexpr int baseH = 680;

    explicit Panel (VeloursProcessor&);
    ~Panel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (float)> onScaleChange;

private:
    void timerCallback() override;
    void refreshPresetBox();
    void stepPreset (int step);
    void savePresetDialog();
    void attachToggle (juce::ToggleButton&, const juce::String& id, const juce::String& text, const juce::String& tip);
    void sectionTitle (juce::Graphics&, juce::Rectangle<int>, const juce::String&);

    VeloursProcessor& proc;
    VeloursLookAndFeel lnf;

    SpectrumDisplay display;
    BandStrip strip;

    Knob depth, detail, attack, release;
    Knob detailTilt, attackTilt, releaseTilt, maxCut, wetTrim, link, focus;
    HSlider mix, output;
    std::unique_ptr<Segments> modeSeg, stereoSeg;
    juce::ToggleButton sidechain, delta, bypass, renderUltra;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    LevelReadout level;
    juce::Label scInfo;

    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" },
                     slotA { "A" }, slotB { "B" }, copyBtn { "A>B" }, undoBtn { "UNDO" }, redoBtn { "REDO" };
    juce::ComboBox presetBox, resolutionBox, qualityBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> resolutionAtt, qualityAtt;
    juce::String shownPreset;
    juce::Array<juce::File> userFiles;

    juce::Rectangle<int> leftArea, rightArea, footerArea, topArea;
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
    juce::TooltipWindow tooltips { this, 700 };
    float scale = 1.0f;
};

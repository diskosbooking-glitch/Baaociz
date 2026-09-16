#include "PluginEditor.h"

namespace colours
{
    const juce::Colour bg        { 0xff0d0d0d };
    const juce::Colour panel     { 0xff161616 };
    const juce::Colour line      { 0xff2e2e2e };
    const juce::Colour dim       { 0xff6b6b6b };
    const juce::Colour mid       { 0xffa8a8a8 };
    const juce::Colour ink       { 0xfff2f2f2 };
}

static juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

// ============================================================================
//  Look & feel
// ============================================================================
AocizLookAndFeel::AocizLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colours::line);
    setColour (juce::Label::textColourId, colours::mid);
    setColour (juce::TextButton::textColourOffId, colours::mid);
    setColour (juce::TextButton::textColourOnId, colours::bg);
    setColour (juce::ToggleButton::textColourId, colours::mid);
}

void AocizLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = 3.0f;

    juce::Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::line);
    g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc;
    valArc.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    g.setColour (colours::ink);
    g.strokePath (valArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - track * 4.0f;
    g.setColour (colours::panel);
    g.fillEllipse (centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f);
    g.setColour (colours::line);
    g.drawEllipse (centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -inner + 4.0f, 3.0f, inner * 0.45f, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (colours::ink);
    g.fillPath (pointer);
}

void AocizLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState())
    {
        g.setColour (colours::ink);
        g.fillRoundedRectangle (r, 3.0f);
    }
    else
    {
        g.setColour (highlighted ? colours::dim : colours::line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
}

void AocizLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? colours::bg : colours::mid);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

void AocizLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (6.0f);
    g.setColour (highlighted ? colours::dim : colours::line);
    g.drawRoundedRectangle (box, 2.0f, 1.0f);
    if (b.getToggleState())
    {
        g.setColour (colours::ink);
        g.fillRoundedRectangle (box.reduced (3.5f), 1.5f);
    }
    g.setColour (b.getToggleState() ? colours::ink : colours::mid);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
}

// ============================================================================
//  Analyseur
// ============================================================================
SpectrumView::SpectrumView (SubShaperProcessor& p) : proc (p)
{
    scratch.resize ((size_t) SubShaperProcessor::fifoSize);
    smoothed.fill (-100.0f);
    startTimerHz (30);
}

void SpectrumView::timerCallback()
{
    auto& fifo = proc.analyserFifo;
    int available = fifo.getNumReady();
    if (available <= 0) { repaint(); return; }

    const auto scope = fifo.read (available);
    int n = 0;
    for (int k = 0; k < scope.blockSize1; ++k) scratch[(size_t) n++] = proc.analyserData[(size_t) (scope.startIndex1 + k)];
    for (int k = 0; k < scope.blockSize2; ++k) scratch[(size_t) n++] = proc.analyserData[(size_t) (scope.startIndex2 + k)];

    if (n >= fftSize)
        std::copy (scratch.begin() + (n - fftSize), scratch.begin() + n, ring.begin());
    else
    {
        std::move (ring.begin() + n, ring.end(), ring.begin());
        std::copy (scratch.begin(), scratch.begin() + n, ring.end() - n);
    }

    std::fill (work.begin(), work.end(), 0.0f);
    std::copy (ring.begin(), ring.end(), work.begin());
    window.multiplyWithWindowingTable (work.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (work.data());

    for (size_t i = 0; i < smoothed.size(); ++i)
    {
        const float db = juce::Decibels::gainToDecibels (work[i] * 4.0f / (float) fftSize, -100.0f);
        smoothed[i] = db > smoothed[i] ? db : smoothed[i] + 0.35f * (db - smoothed[i]);
    }
    repaint();
}

float SpectrumView::xForFreq (float hz) const
{
    const float norm = std::log (hz / 20.0f) / std::log (20000.0f / 20.0f);
    return norm * (float) getWidth();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 4.0f);

    const float w = bounds.getWidth(), h = bounds.getHeight();
    const float minDb = -90.0f, maxDb = 0.0f;
    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, h, 0.0f); };

    // Zone grave (sous la coupure)
    const float xover = proc.apvts.getRawParameterValue ("crossover")->load();
    const float xx = xForFreq (xover);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRect (0.0f, 0.0f, xx, h);

    // Grille
    g.setFont (juce::FontOptions (10.0f));
    for (float f : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (colours::line);
        g.drawVerticalLine ((int) x, 0.0f, h);
        g.setColour (colours::dim);
        const auto txt = f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f);
        g.drawText (txt, (int) x + 3, (int) h - 14, 30, 12, juce::Justification::left);
    }
    for (float db : { -72.0f, -54.0f, -36.0f, -18.0f })
    {
        g.setColour (colours::line.withAlpha (0.6f));
        g.drawHorizontalLine ((int) yForDb (db), 0.0f, w);
    }

    // Courbe
    const double sr = proc.getCurrentSampleRate();
    juce::Path curve;
    bool started = false;
    float firstX = 0.0f;
    for (size_t i = 1; i < smoothed.size(); ++i)
    {
        const float hz = (float) (i * sr / fftSize);
        if (hz < 20.0f) continue;
        if (hz > 20000.0f) break;
        const float x = xForFreq (hz), y = yForDb (smoothed[i]);
        if (! started) { curve.startNewSubPath (x, y); firstX = x; started = true; }
        else curve.lineTo (x, y);
    }
    if (started)
    {
        juce::Path fill (curve);
        fill.lineTo (w, h);
        fill.lineTo (firstX, h);
        fill.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillPath (fill);
        g.setColour (colours::ink);
        g.strokePath (curve, juce::PathStrokeType (1.4f));
    }

    // Ligne de coupure
    g.setColour (colours::ink);
    g.drawVerticalLine ((int) xx, 0.0f, h);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (juce::String ((int) std::round (xover)) + " Hz", (int) xx + 5, 6, 70, 14, juce::Justification::left);
}

// ============================================================================
//  Potard
// ============================================================================
LabelledKnob::LabelledKnob()
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, colours::ink);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void LabelledKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

// ============================================================================
//  Éditeur
// ============================================================================
SubShaperEditor::SubShaperEditor (SubShaperProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (spectrum);

    // Modes
    const juce::StringArray names { "SATURER", utf8 ("RÉSONNER"), "OCTAVE", utf8 ("SYNTHÈSE"), "REPLI" };
    auto* modeParam = proc.apvts.getParameter ("mode");
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = modeButtons.add (new juce::TextButton (names[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { if (modeAttachment) modeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*modeParam,
        [this] (float v) { updateModeUI ((int) std::lround (v)); });
    modeAttachment->sendInitialUpdate();

    // Potards
    auto setupKnob = [this] (LabelledKnob& k, const juce::String& name, const juce::String& id,
                             std::unique_ptr<SliderAtt>& att)
    {
        k.label.setText (name, juce::dontSendNotification);
        addAndMakeVisible (k);
        att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    };
    setupKnob (crossoverKnob, "COUPURE",           "crossover", crossoverAtt);
    setupKnob (amountKnob,    utf8 ("INTENSITÉ"), "amount",    amountAtt);
    setupKnob (characterKnob, utf8 ("CARACTÈRE"), "character", characterAtt);
    setupKnob (mixKnob,       utf8 ("MÉLANGE"),   "mix",       mixAtt);
    setupKnob (outputKnob,    "SORTIE",            "output",    outputAtt);

    crossoverKnob.slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + " Hz"; };
    outputKnob.slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    for (auto* k : { &amountKnob, &characterKnob, &mixKnob })
        k->slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + " %"; };
    for (auto* k : { &crossoverKnob, &amountKnob, &characterKnob, &mixKnob, &outputKnob })
        k->slider.updateText();

    crossoverKnob.slider.onValueChange = [this] { spectrum.repaint(); };

    // Interrupteurs
    monoLowBtn.setButtonText ("Grave mono");
    soloLowBtn.setButtonText ("Solo grave");
    subCutBtn.setButtonText (utf8 ("Coupe < 25 Hz"));
    for (auto* b : { &monoLowBtn, &soloLowBtn, &subCutBtn })
        addAndMakeVisible (b);
    monoLowAtt = std::make_unique<ButtonAtt> (proc.apvts, "monoLow", monoLowBtn);
    soloLowAtt = std::make_unique<ButtonAtt> (proc.apvts, "soloLow", soloLowBtn);
    subCutAtt  = std::make_unique<ButtonAtt> (proc.apvts, "subCut", subCutBtn);

    modeHint.setJustificationType (juce::Justification::topLeft);
    modeHint.setFont (juce::FontOptions (12.0f));
    modeHint.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (modeHint);

    updateModeUI ((int) proc.apvts.getRawParameterValue ("mode")->load());
    setSize (820, 470);
}

SubShaperEditor::~SubShaperEditor()
{
    setLookAndFeel (nullptr);
}

void SubShaperEditor::updateModeUI (int mode)
{
    for (int i = 0; i < modeButtons.size(); ++i)
        modeButtons[i]->setToggleState (i == mode, juce::dontSendNotification);

    struct Info { const char* amount; const char* character; const char* hint; };
    static const Info infos[] = {
        { "DRIVE",        "ASYMÉTRIE",    "Saturation douce du grave : ajoute des harmoniques pour que la basse passe sur petites enceintes. Caractère = harmoniques paires." },
        { "RÉSONANCE",    "FRÉQUENCE",    "Pic résonant accordable entre 30 et 200 Hz pour appuyer la fondamentale. Caractère = fréquence du pic." },
        { "NIVEAU SUB",   "TONALITÉ",     "Génère un sub une octave sous le signal. Caractère = rondeur (bas) ou présence (haut) du sub." },
        { "NIVEAU SINUS", "REMPLACEMENT", "Suit la hauteur et génère un sinus pur. Caractère = 0 : ajouté ; 100 : remplace le grave d'origine." },
        { "REPLIS",       "ASYMÉTRIE",    "Wavefolder agressif pour basses sales. Niveau compensé automatiquement." },
    };
    const auto& info = infos[juce::jlimit (0, 4, mode)];
    amountKnob.label.setText (utf8 (info.amount), juce::dontSendNotification);
    characterKnob.label.setText (utf8 (info.character), juce::dontSendNotification);
    modeHint.setText (utf8 (info.hint), juce::dontSendNotification);
}

void SubShaperEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    auto header = getLocalBounds().removeFromTop (46).reduced (20, 0);
    g.setColour (colours::ink);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("AOCIZ", header, juce::Justification::centredLeft);
    g.setColour (colours::mid);
    g.setFont (juce::FontOptions (20.0f));
    g.drawText ("SUBSHAPER", header.withTrimmedLeft (76), juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" + juce::String (JucePlugin_VersionString), header, juce::Justification::centredRight);

    g.setColour (colours::line);
    g.drawHorizontalLine (46, 20.0f, (float) getWidth() - 20.0f);

    // séparateur colonne droite
    g.drawVerticalLine (626, 250.0f, (float) getHeight() - 20.0f);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("OUTILS", 646, 250, 150, 16, juce::Justification::left);
}

void SubShaperEditor::resized()
{
    spectrum.setBounds (20, 58, getWidth() - 40, 170);

    auto modeRow = juce::Rectangle<int> (20, 250, 586, 30);
    const int bw = (modeRow.getWidth() - 4 * 6) / 5;
    for (auto* b : modeButtons)
    {
        b->setBounds (modeRow.removeFromLeft (bw));
        modeRow.removeFromLeft (6);
    }

    auto knobs = juce::Rectangle<int> (20, 294, 586, 124);
    const int kw = knobs.getWidth() / 5;
    for (auto* k : { &crossoverKnob, &amountKnob, &characterKnob, &mixKnob, &outputKnob })
        k->setBounds (knobs.removeFromLeft (kw).reduced (4, 0));

    modeHint.setBounds (20, 424, 586, 36);

    auto tools = juce::Rectangle<int> (642, 272, 160, 90);
    for (auto* b : { &monoLowBtn, &soloLowBtn, &subCutBtn })
        b->setBounds (tools.removeFromTop (30));
}

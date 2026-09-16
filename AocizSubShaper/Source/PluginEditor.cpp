#include "PluginEditor.h"

static juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

void setAccent (juce::Component& c, juce::Colour col)
{
    c.getProperties().set ("accent", (juce::int64) col.getARGB());
}

juce::Colour getAccent (const juce::Component& c, juce::Colour fallback)
{
    const auto v = c.getProperties()["accent"];
    return v.isVoid() ? fallback : juce::Colour ((juce::uint32) (juce::int64) v);
}

// ============================================================================
//  Look & feel
// ============================================================================
ModularLookAndFeel::ModularLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, palette::edge);
    setColour (juce::Label::textColourId, palette::text);
}

juce::Font ModularLookAndFeel::getLabelFont (juce::Label& l)
{
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
        return juce::Font (juce::FontOptions (15.0f, juce::Font::bold));
    return LookAndFeel_V4::getLabelFont (l);
}

void ModularLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto col = getAccent (s);
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // graduations
    g.setColour (palette::textDim.withAlpha (0.7f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto p1 = c.getPointOnCircumference (r * 0.94f, a);
        const auto p2 = c.getPointOnCircumference (r, a);
        g.drawLine ({ p1, p2 }, i % 5 == 0 ? 2.0f : 1.2f);
    }

    // arc de valeur
    const float arcR = r * 0.83f, arcW = r * 0.075f;
    juce::Path bgArc, valArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    valArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (juce::Colour (0xff2c2e35));
    g.strokePath (bgArc, stroke);
    g.setColour (col.withAlpha (0.35f));
    g.strokePath (valArc, juce::PathStrokeType (arcW * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (valArc, stroke);

    // ombre
    const float skirtR = r * 0.68f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - skirtR, c.y - skirtR + r * 0.06f, skirtR * 2.0f, skirtR * 2.0f);

    // jupe crantée
    g.setColour (col.darker (0.75f));
    g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);
    g.setColour (col.darker (1.3f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 28.0f + angle;
        g.drawLine ({ c.getPointOnCircumference (skirtR * 0.82f, a), c.getPointOnCircumference (skirtR * 0.99f, a) }, 1.6f);
    }

    // capuchon
    const float capR = r * 0.52f;
    juce::ColourGradient grad (col.brighter (0.45f), c.x, c.y - capR, col.darker (0.35f), c.x, c.y + capR, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillEllipse (c.x - capR * 0.6f, c.y - capR * 0.85f, capR * 1.2f, capR * 0.6f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.2f);

    // index
    const auto pA = c.getPointOnCircumference (skirtR * 0.12f, angle);
    const auto pB = c.getPointOnCircumference (skirtR * 0.95f, angle);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawLine ({ pA, pB }, r * 0.09f);
    g.setColour (juce::Colours::white);
    g.drawLine ({ pA, pB }, r * 0.055f);
}

static void drawLedPanel (juce::Graphics& g, juce::Button& b, juce::Colour col, bool on, bool highlighted, float fontSize)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff15161b));
    g.fillRoundedRectangle (r, 7.0f);
    if (on)
    {
        g.setColour (col.withAlpha (0.2f));
        g.fillRoundedRectangle (r, 7.0f);
    }
    g.setColour (on ? col : (highlighted ? palette::textDim : palette::edge));
    g.drawRoundedRectangle (r, 7.0f, on ? 2.0f : 1.2f);

    const float ledR = juce::jmin (7.0f, r.getHeight() * 0.17f);
    const juce::Point<float> led (r.getX() + 12.0f + ledR, r.getCentreY());
    if (on)
    {
        g.setColour (col.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (ledR * 3.4f, ledR * 3.4f).withCentre (led));
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (ledR * 2.0f, ledR * 2.0f).withCentre (led));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillEllipse (juce::Rectangle<float> (ledR * 0.8f, ledR * 0.8f).withCentre (led.translated (-ledR * 0.25f, -ledR * 0.25f)));
    }
    else
    {
        g.setColour (col.darker (1.4f));
        g.fillEllipse (juce::Rectangle<float> (ledR * 2.0f, ledR * 2.0f).withCentre (led));
        g.setColour (col.withAlpha (0.45f));
        g.drawEllipse (juce::Rectangle<float> (ledR * 2.0f, ledR * 2.0f).withCentre (led), 1.0f);
    }

    g.setColour (on ? palette::text : juce::Colour (0xffb8bac2));
    g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
    auto textArea = r.withTrimmedLeft (ledR * 2.0f + 22.0f).withTrimmedRight (6.0f).toNearestInt();
    g.drawFittedText (b.getButtonText(), textArea, juce::Justification::centredLeft, 1);
}

void ModularLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                               bool highlighted, bool)
{
    drawLedPanel (g, b, getAccent (b), b.getToggleState(), highlighted, 15.0f);
}

void ModularLookAndFeel::drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) {}

void ModularLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    drawLedPanel (g, b, getAccent (b), b.getToggleState(), highlighted, 13.5f);
}

// ============================================================================
//  Écran d'analyse
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
    const int available = fifo.getNumReady();
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
    return std::log (hz / 20.0f) / std::log (20000.0f / 20.0f) * (float) getWidth();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth(), h = bounds.getHeight();
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const auto col = palette::modeColour (mode);

    g.setColour (palette::screen);
    g.fillRoundedRectangle (bounds, 8.0f);

    const float minDb = -90.0f, maxDb = 0.0f;
    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, h - 6.0f, 6.0f); };

    const float xover = proc.apvts.getRawParameterValue ("crossover")->load();
    const float xx = xForFreq (xover);
    g.setColour (palette::cyan.withAlpha (0.07f));
    g.fillRect (0.0f, 0.0f, xx, h);

    g.setFont (juce::FontOptions (11.0f));
    for (float f : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (palette::grid);
        g.drawVerticalLine ((int) x, 0.0f, h);
        g.setColour (palette::textDim);
        const auto txt = f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f);
        g.drawText (txt, (int) x + 4, (int) h - 17, 34, 13, juce::Justification::left);
    }
    for (float db : { -72.0f, -54.0f, -36.0f, -18.0f })
    {
        g.setColour (palette::grid);
        g.drawHorizontalLine ((int) yForDb (db), 0.0f, w);
    }

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
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.35f), 0.0f, 0.0f, col.withAlpha (0.02f), 0.0f, h, false));
        g.fillPath (fill);
        g.setColour (col.withAlpha (0.35f));
        g.strokePath (curve, juce::PathStrokeType (4.0f));
        g.setColour (col.brighter (0.3f));
        g.strokePath (curve, juce::PathStrokeType (1.6f));
    }

    // ligne de coupure
    juce::Path dashed, line;
    line.startNewSubPath (xx, 0.0f);
    line.lineTo (xx, h);
    const float dashes[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, line, dashes, 2);
    g.setColour (palette::cyan);
    g.fillPath (dashed);

    auto tag = juce::Rectangle<float> (xx + 6.0f, 8.0f, 66.0f, 20.0f);
    g.setColour (palette::cyan);
    g.fillRoundedRectangle (tag, 4.0f);
    g.setColour (palette::screen);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (juce::String ((int) std::round (xover)) + " Hz", tag.toNearestInt(), juce::Justification::centred);

    g.setColour (palette::edge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.5f);
}

// ============================================================================
//  Potard
// ============================================================================
LabelledKnob::LabelledKnob()
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 22);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, palette::text);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setDoubleClickReturnValue (false, 0.0);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void LabelledKnob::setColour (juce::Colour c)
{
    setAccent (slider, c);
    label.setColour (juce::Label::textColourId, c);
}

void LabelledKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (20));
    slider.setBounds (r);
}

// ============================================================================
//  Éditeur
// ============================================================================
static constexpr int kW = 940, kH = 590;

SubShaperEditor::SubShaperEditor (SubShaperProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (spectrum);

    const juce::StringArray names { "SATURATE", "RESONATE", "OCTAVE", "SYNTHESIZE", "FOLD" };
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = modeButtons.add (new juce::TextButton (names[i]));
        setAccent (*b, palette::modeColour (i));
        b->setClickingTogglesState (false);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        b->onClick = [this, i] { if (modeAttachment) modeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("mode"),
        [this] (float v) { updateModeUI ((int) std::lround (v)); });

    auto setupKnob = [this] (LabelledKnob& k, const juce::String& name, const juce::String& id,
                             juce::Colour col, std::unique_ptr<SliderAtt>& att)
    {
        k.label.setText (name, juce::dontSendNotification);
        k.setColour (col);
        addAndMakeVisible (k);
        att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    };
    setupKnob (crossoverKnob, "CROSSOVER",         "crossover", palette::cyan,   crossoverAtt);
    setupKnob (amountKnob,    "AMOUNT",           "amount",    palette::pink,   amountAtt);
    setupKnob (characterKnob, "CHARACTER",        "character", palette::amber,  characterAtt);
    setupKnob (mixKnob,       "MIX",              "mix",       palette::lime,   mixAtt);
    setupKnob (outputKnob,    "OUTPUT",            "output",    palette::indigo, outputAtt);

    crossoverKnob.slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + " Hz"; };
    outputKnob.slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    for (auto* k : { &amountKnob, &characterKnob, &mixKnob })
        k->slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + " %"; };
    for (auto* k : { &crossoverKnob, &amountKnob, &characterKnob, &mixKnob, &outputKnob })
        k->slider.updateText();
    crossoverKnob.slider.onValueChange = [this] { spectrum.repaint(); };

    monoLowBtn.setButtonText ("MONO LOW");
    soloLowBtn.setButtonText ("SOLO LOW");
    subCutBtn.setButtonText ("SUB CUT < 25 Hz");
    setAccent (monoLowBtn, palette::amber);
    setAccent (soloLowBtn, palette::red);
    setAccent (subCutBtn, palette::cyan);
    for (auto* b : { &monoLowBtn, &soloLowBtn, &subCutBtn })
    {
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (b);
    }
    monoLowAtt = std::make_unique<ButtonAtt> (proc.apvts, "monoLow", monoLowBtn);
    soloLowAtt = std::make_unique<ButtonAtt> (proc.apvts, "soloLow", soloLowBtn);
    subCutAtt  = std::make_unique<ButtonAtt> (proc.apvts, "subCut", subCutBtn);

    modeHint.setJustificationType (juce::Justification::centredLeft);
    modeHint.setFont (juce::FontOptions (13.0f));
    modeHint.setColour (juce::Label::textColourId, palette::textDim);
    addAndMakeVisible (modeHint);

    modeAttachment->sendInitialUpdate();
    setSize (kW, kH);
}

SubShaperEditor::~SubShaperEditor()
{
    setLookAndFeel (nullptr);
}

void SubShaperEditor::updateModeUI (int mode)
{
    currentMode = juce::jlimit (0, 4, mode);
    for (int i = 0; i < modeButtons.size(); ++i)
        modeButtons[i]->setToggleState (i == currentMode, juce::dontSendNotification);

    struct Info { const char* amount; const char* character; const char* hint; };
    static const Info infos[] = {
        { "DRIVE",        "ASYMMETRY",    "Low-end saturation: adds harmonics so the bass translates on small speakers. Character = even harmonics." },
        { "RESONANCE",    "FREQUENCY",    "Tunable resonant peak from 30 to 200 Hz to reinforce the fundamental. Character = peak frequency." },
        { "SUB LEVEL",    "TONE",         "Generates a sub one octave below the signal. Character = round (low) or present (high) sub." },
        { "SINE LEVEL",   "REPLACE",      "Tracks pitch and generates a pure sine. Character 0 = added, 100 = replaces the original low end." },
        { "FOLDS",        "ASYMMETRY",    "Aggressive wavefolder for dirty basses. Output level is compensated automatically." },
    };
    const auto& info = infos[currentMode];
    amountKnob.label.setText (utf8 (info.amount), juce::dontSendNotification);
    characterKnob.label.setText (utf8 (info.character), juce::dontSendNotification);
    modeHint.setText (utf8 (info.hint), juce::dontSendNotification);
    modeHint.setColour (juce::Label::textColourId, palette::modeColour (currentMode).interpolatedWith (palette::text, 0.45f));
    repaint();
    spectrum.repaint();
}

void SubShaperEditor::drawRail (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9bdc6), 0.0f, r.getY(),
                                             juce::Colour (0xff6d717a), 0.0f, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX(), r.getRight());
    for (float x : { 18.0f, r.getRight() - 18.0f, r.getCentreX() })
    {
        auto hole = juce::Rectangle<float> (14.0f, 7.0f).withCentre ({ x, r.getCentreY() });
        g.setColour (juce::Colour (0xff2a2c31));
        g.fillRoundedRectangle (hole, 3.5f);
        g.setColour (juce::Colour (0xffd9dce2));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x, r.getCentreY() }));
        g.setColour (juce::Colour (0xff55585f));
        g.drawLine (x - 3.0f, r.getCentreY(), x + 3.0f, r.getCentreY(), 1.4f);
    }
}

void SubShaperEditor::drawJack (juce::Graphics& g, juce::Point<float> c, const juce::String& name)
{
    g.setColour (palette::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (name, juce::Rectangle<float> (60.0f, 14.0f).withCentre (c.translated (0.0f, -26.0f)), juce::Justification::centred);
    juce::Path nut;
    nut.addPolygon (c, 6, 16.0f, 0.0f);
    g.setColour (juce::Colour (0xffc3c7cf));
    g.fillPath (nut);
    g.setColour (juce::Colour (0xff7d8189));
    g.strokePath (nut, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colour (0xff0e0f12));
    g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c));
}

void SubShaperEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (palette::panelTop, 0.0f, 0.0f, palette::panelBottom, 0.0f, (float) kH, false));
    g.fillAll();

    drawRail (g, { 0.0f, 0.0f, (float) kW, 20.0f });
    drawRail (g, { 0.0f, (float) kH - 20.0f, (float) kW, 20.0f });

    // Titre : lettres colorées
    const juce::String title ("SUBSHAPER");
    const juce::Colour letters[] { palette::red, palette::orange, palette::amber, palette::lime, palette::green,
                                   palette::cyan, palette::blue, palette::indigo, palette::pink };
    juce::Font titleFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.setFont (titleFont);
    float tx = 26.0f;
    for (int i = 0; i < title.length(); ++i)
    {
        const auto ch = title.substring (i, i + 1);
        const float cw = juce::GlyphArrangement::getStringWidth (titleFont, ch);
        g.setColour (letters[i]);
        g.drawText (ch, juce::Rectangle<float> (tx, 26.0f, cw + 2.0f, 40.0f), juce::Justification::centredLeft, false);
        tx += cw + 3.0f;
    }
    g.setColour (palette::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("BASS PROCESSOR", juce::Rectangle<float> (tx + 14.0f, 38.0f, 200.0f, 18.0f), juce::Justification::centredLeft);
    g.drawText ("v" + juce::String (JucePlugin_VersionString),
                juce::Rectangle<float> ((float) kW - 126.0f, 38.0f, 100.0f, 18.0f), juce::Justification::centredRight);

    // Modules
    auto module = [&] (juce::Rectangle<float> r, const juce::String& name, juce::Colour col)
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (palette::edge);
        g.drawRoundedRectangle (r, 10.0f, 1.2f);
        auto tab = juce::Rectangle<float> (r.getX() + 16.0f, r.getY() - 9.0f, 10.0f + (float) name.length() * 8.5f, 18.0f);
        g.setColour (col);
        g.fillRoundedRectangle (tab, 9.0f);
        g.setColour (palette::panelBottom);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (name, tab.toNearestInt(), juce::Justification::centred);
    };
    module ({ 14.0f, 272.0f, 712.0f, 294.0f }, "MODULE", palette::modeColour (currentMode));
    module ({ 740.0f, 272.0f, 186.0f, 294.0f }, "TOOLS", palette::amber);

    // Prises décoratives + câble
    const juce::Point<float> jIn (788.0f, 500.0f), jOut (878.0f, 500.0f);
    drawJack (g, jIn, "IN");
    drawJack (g, jOut, "OUT");
    juce::Path cable;
    cable.startNewSubPath (jIn);
    cable.cubicTo (jIn.translated (8.0f, 58.0f), jOut.translated (-8.0f, 58.0f), jOut);
    const auto cableCol = palette::modeColour (currentMode);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (cable, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (0.0f, 3.0f));
    g.setColour (cableCol.darker (0.2f));
    g.strokePath (cable, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cableCol.brighter (0.4f).withAlpha (0.6f));
    g.strokePath (cable, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (0.0f, -1.5f));
    for (auto pt : { jIn, jOut })
    {
        g.setColour (cableCol.darker (0.5f));
        g.fillRoundedRectangle (juce::Rectangle<float> (11.0f, 16.0f).withCentre (pt.translated (0.0f, 5.0f)), 3.0f);
        g.setColour (juce::Colour (0xff1a1b20));
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt));
    }
}

void SubShaperEditor::resized()
{
    spectrum.setBounds (20, 76, kW - 40, 172);

    auto modeRow = juce::Rectangle<int> (30, 292, 680, 46);
    const int gap = 10;
    const int bw = (modeRow.getWidth() - 4 * gap) / 5;
    for (auto* b : modeButtons)
    {
        b->setBounds (modeRow.removeFromLeft (bw));
        modeRow.removeFromLeft (gap);
    }

    auto knobs = juce::Rectangle<int> (30, 352, 680, 172);
    const int kw = knobs.getWidth() / 5;
    for (auto* k : { &crossoverKnob, &amountKnob, &characterKnob, &mixKnob, &outputKnob })
        k->setBounds (knobs.removeFromLeft (kw));

    modeHint.setBounds (30, 528, 680, 30);

    auto tools = juce::Rectangle<int> (754, 292, 158, 132);
    for (auto* b : { &monoLowBtn, &soloLowBtn, &subCutBtn })
    {
        b->setBounds (tools.removeFromTop (38));
        tools.removeFromTop (9);
    }
}

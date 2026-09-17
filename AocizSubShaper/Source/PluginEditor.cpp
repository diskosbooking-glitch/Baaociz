#include "PluginEditor.h"
#include <complex>

void setAccent (juce::Component& c, juce::Colour col)
{
    c.getProperties().set ("accent", (juce::int64) col.getARGB());
}

juce::Colour getAccent (const juce::Component& c, juce::Colour fallback)
{
    const auto v = c.getProperties()["accent"];
    return v.isVoid() ? fallback : juce::Colour ((juce::uint32) (juce::int64) v);
}

static juce::Font bold (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
static juce::Font plain (float size) { return juce::Font (juce::FontOptions (size)); }

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
    setColour (juce::ComboBox::textColourId, palette::text);
    setColour (juce::ComboBox::arrowColourId, palette::textDim);
    setColour (juce::PopupMenu::backgroundColourId, palette::card);
    setColour (juce::PopupMenu::textColourId, palette::text);
    setColour (juce::PopupMenu::headerTextColourId, palette::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::sky.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, palette::text);
    setColour (juce::TooltipWindow::backgroundColourId, palette::card);
    setColour (juce::TooltipWindow::textColourId, palette::text);
    setColour (juce::TooltipWindow::outlineColourId, palette::edge);
    setColour (juce::AlertWindow::backgroundColourId, palette::card);
    setColour (juce::AlertWindow::textColourId, palette::text);
    setColour (juce::AlertWindow::outlineColourId, palette::edge);
    setColour (juce::TextEditor::backgroundColourId, palette::screen);
    setColour (juce::TextEditor::textColourId, palette::text);
    setColour (juce::TextEditor::outlineColourId, palette::edge);
    setColour (juce::TextEditor::focusedOutlineColourId, palette::sky);
}

juce::Font ModularLookAndFeel::getLabelFont (juce::Label& l)
{
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
        return bold (13.0f);
    return LookAndFeel_V4::getLabelFont (l);
}

juce::Font ModularLookAndFeel::getComboBoxFont (juce::ComboBox&) { return bold (13.0f); }
juce::Font ModularLookAndFeel::getPopupMenuFont() { return plain (14.0f); }

void ModularLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (palette::screen);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver (true) ? palette::textDim : palette::edge);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    juce::Path arrow;
    const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (palette::textDim);
    g.fillPath (arrow);
}

void ModularLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void ModularLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto col = getAccent (s);
    const auto stepLabels = juce::StringArray::fromTokens (s.getProperties()["steps"].toString(), "|", "");
    const bool stepped = stepLabels.size() > 1 && stepLabels[0].isNotEmpty();

    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    if (stepped)
    {
        // Étiquettes des crans autour du potard
        const float labelR = r - 8.0f;
        const int n = stepLabels.size();
        const int current = juce::roundToInt (pos * (float) (n - 1));
        g.setFont (bold (juce::jlimit (9.0f, 13.0f, r * 0.16f)));
        for (int i = 0; i < n; ++i)
        {
            const float a = startAngle + (float) i / (float) (n - 1) * (endAngle - startAngle);
            const auto pt = c.getPointOnCircumference (labelR, a);
            g.setColour (i == current ? col : palette::textDim);
            g.drawText (stepLabels[i], juce::Rectangle<float> (30.0f, 14.0f).withCentre (pt), juce::Justification::centred);
        }
        r = labelR - 12.0f;
    }
    else
    {
        g.setColour (palette::textDim.withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            g.drawLine ({ c.getPointOnCircumference (r * 0.93f, a), c.getPointOnCircumference (r, a) }, i % 5 == 0 ? 1.8f : 1.0f);
        }
    }

    const float arcR = r * (stepped ? 0.98f : 0.83f), arcW = juce::jmax (2.5f, r * 0.07f);
    const juce::PathStrokeType stroke (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff34333b));
    g.strokePath (bgArc, stroke);

    if (stepped)
    {
        // Point sur le cran actif
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (arcW * 2.2f, arcW * 2.2f).withCentre (c.getPointOnCircumference (arcR, angle)));
    }
    else
    {
        // Arc de valeur (bipolaire si la plage part du négatif)
        const bool bipolar = s.getMinimum() < 0.0;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        juce::Path valArc;
        valArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (col);
        g.strokePath (valArc, stroke);
    }

    const float skirtR = r * (stepped ? 0.84f : 0.68f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - skirtR, c.y - skirtR + r * 0.05f, skirtR * 2.0f, skirtR * 2.0f);
    g.setColour (col.darker (0.55f).withSaturation (col.getSaturation() * 0.7f));
    g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);
    g.setColour (col.darker (0.95f).withAlpha (0.8f));
    const int notches = stepped ? 12 : 24;
    for (int i = 0; i < notches; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) notches + angle;
        g.drawLine ({ c.getPointOnCircumference (skirtR * 0.8f, a), c.getPointOnCircumference (skirtR * 0.98f, a) },
                    stepped ? 2.4f : 1.4f);
    }

    const float capR = skirtR * 0.76f;
    g.setGradientFill (juce::ColourGradient (col.brighter (0.25f), c.x, c.y - capR, col.darker (0.15f), c.x, c.y + capR, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.fillEllipse (c.x - capR * 0.6f, c.y - capR * 0.85f, capR * 1.2f, capR * 0.55f);

    const auto pA = c.getPointOnCircumference (skirtR * 0.15f, angle);
    const auto pB = c.getPointOnCircumference (skirtR * 0.95f, angle);
    g.setColour (palette::ink);
    g.drawLine ({ pA, pB }, juce::jmax (2.0f, r * 0.06f));
}

static void drawLedPanel (juce::Graphics& g, juce::Button& b, juce::Colour col, bool on, bool over, float fontSize)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (palette::screen);
    g.fillRoundedRectangle (r, 6.0f);
    if (on)
    {
        g.setColour (col.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 6.0f);
    }
    g.setColour (on ? col : (over ? palette::textDim : palette::edge));
    g.drawRoundedRectangle (r, 6.0f, on ? 1.8f : 1.1f);

    const float ledR = juce::jmin (6.0f, r.getHeight() * 0.17f);
    const juce::Point<float> led (r.getX() + 10.0f + ledR, r.getCentreY());
    if (on)
    {
        g.setColour (col.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (ledR * 3.4f, ledR * 3.4f).withCentre (led));
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (ledR * 2.0f, ledR * 2.0f).withCentre (led));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (ledR * 0.7f, ledR * 0.7f).withCentre (led.translated (-ledR * 0.3f, -ledR * 0.3f)));
    }
    else
    {
        g.setColour (col.withAlpha (0.18f));
        g.fillEllipse (juce::Rectangle<float> (ledR * 2.0f, ledR * 2.0f).withCentre (led));
        g.setColour (col.withAlpha (0.5f));
        g.drawEllipse (juce::Rectangle<float> (ledR * 2.0f, ledR * 2.0f).withCentre (led), 1.0f);
    }

    g.setColour (on ? palette::text : juce::Colour (0xffb5b2bf));
    g.setFont (bold (fontSize));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (ledR * 2.0f + 18.0f).withTrimmedRight (4.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);
}

void ModularLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto col = getAccent (b);
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);

    if (b.getProperties()["segment"])
    {
        const bool on = b.getToggleState();
        g.setColour (on ? col : palette::screen);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? col : (over ? palette::textDim : palette::edge));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (on ? palette::ink : juce::Colour (0xffb5b2bf));
        g.setFont (bold (juce::jmin (12.5f, r.getHeight() * 0.5f)));
        g.drawFittedText (b.getButtonText(), r.toNearestInt().reduced (2, 0), juce::Justification::centred, 1);
        return;
    }

    // Bouton d'action simple
    g.setColour (down ? col.withAlpha (0.35f) : palette::screen);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (over ? col : palette::edge);
    g.drawRoundedRectangle (r, 6.0f, 1.1f);
    g.setColour (b.isEnabled() ? palette::text : palette::textDim.withAlpha (0.5f));
    g.setFont (bold (12.5f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().reduced (4, 0), juce::Justification::centred, 1);
}

void ModularLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const float fs = b.getProperties().contains ("fontSize") ? (float) b.getProperties()["fontSize"] : 12.5f;
    drawLedPanel (g, b, getAccent (b), b.getToggleState(), over, fs);
}

// ============================================================================
//  Analyseur + clavier
// ============================================================================
SpectrumView::SpectrumView (SubShaperProcessor& p) : proc (p)
{
    scratch.resize ((size_t) SubShaperProcessor::fifoSize);
    smoothed.fill (-100.0f);
    startTimerHz (30);
}

void SpectrumView::setFocus (const juce::String& id, bool hold)
{
    if (hold)
    {
        focusId = id;
        focusAlpha = 1.0f;
        focusHold = true;
    }
    else if (id == focusId)
    {
        focusHold = false;
    }
}

void SpectrumView::flash (const juce::String& id)
{
    if (focusHold && id != focusId) return;
    focusId = id;
    focusAlpha = 1.0f;
}

void SpectrumView::timerCallback()
{
    if (! focusHold)
        focusAlpha = juce::jmax (0.0f, focusAlpha - 0.035f);

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

float SpectrumView::xForFreq (float hz, float w) const
{
    return std::log (hz / 20.0f) / std::log (20000.0f / 20.0f) * w;
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float stripH = 30.0f;
    const float h = bounds.getHeight() - stripH;

    g.setColour (palette::screen);
    g.fillRoundedRectangle (bounds, 8.0f);

    const float minDb = -90.0f, maxDb = 0.0f;
    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, h - 4.0f, 8.0f); };

    const float xover = proc.apvts.getRawParameterValue ("crossover")->load();
    const float xx = xForFreq (xover, w);
    g.setColour (palette::sky.withAlpha (0.06f));
    g.fillRect (0.0f, 0.0f, xx, h);

    // Grille : Hz en haut
    g.setFont (plain (10.5f));
    for (float f : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f, w);
        g.setColour (palette::grid);
        g.drawVerticalLine ((int) x, 0.0f, h);
        g.setColour (palette::textDim.withAlpha (0.8f));
        const auto txt = f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f);
        g.drawText (txt, (int) x + 3, 4, 30, 12, juce::Justification::left);
    }
    for (float db : { -72.0f, -54.0f, -36.0f, -18.0f })
    {
        g.setColour (palette::grid);
        g.drawHorizontalLine ((int) yForDb (db), 0.0f, w);
    }

    // Repères KEY et harmonique TONE
    const float keyHz = proc.getKeyFrequency();
    const bool toneOn = proc.apvts.getRawParameterValue ("toneOn")->load() > 0.5f;
    const int harm = (int) proc.apvts.getRawParameterValue ("toneHarm")->load() + 1;
    const float kx = xForFreq (keyHz, w);
    g.setColour (palette::butter.withAlpha (0.8f));
    g.drawVerticalLine ((int) kx, 0.0f, h);
    if (toneOn && harm > 1)
    {
        g.setColour (palette::lavender.withAlpha (0.8f));
        g.drawVerticalLine ((int) xForFreq (keyHz * (float) harm, w), 0.0f, h);
    }

    // Courbe
    const double sr = proc.currentSampleRate.load();
    juce::Path curve;
    bool started = false;
    float firstX = 0.0f;
    for (size_t i = 1; i < smoothed.size(); ++i)
    {
        const float hz = (float) (i * sr / fftSize);
        if (hz < 20.0f) continue;
        if (hz > 20000.0f) break;
        const float x = xForFreq (hz, w), y = yForDb (smoothed[i]);
        if (! started) { curve.startNewSubPath (x, y); firstX = x; started = true; }
        else curve.lineTo (x, y);
    }
    if (started)
    {
        juce::Path fill (curve);
        fill.lineTo (w, h);
        fill.lineTo (firstX, h);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (palette::sky.withAlpha (0.28f), 0.0f, 0.0f,
                                                 palette::lavender.withAlpha (0.03f), 0.0f, h, false));
        g.fillPath (fill);
        g.setColour (palette::sky);
        g.strokePath (curve, juce::PathStrokeType (1.5f));
    }

    drawOverlays (g, w, h);

    // Coupure
    juce::Path dashed, line;
    line.startNewSubPath (xx, 0.0f);
    line.lineTo (xx, h);
    const float dashes[] { 5.0f, 4.0f };
    juce::PathStrokeType (1.4f).createDashedStroke (dashed, line, dashes, 2);
    g.setColour (palette::aqua);
    g.fillPath (dashed);
    auto tag = juce::Rectangle<float> (xx + 6.0f, 20.0f, 64.0f, 18.0f);
    g.setColour (palette::aqua);
    g.fillRoundedRectangle (tag, 4.0f);
    g.setColour (palette::ink);
    g.setFont (bold (11.5f));
    g.drawText (juce::String ((int) std::round (xover)) + " Hz", tag.toNearestInt(), juce::Justification::centred);

    // Clavier (notes C0 à B4) : touches puis étiquettes
    const float stripY = h;
    int keyMidi = 0;
    params::noteNameForFrequency (keyHz, nullptr, &keyMidi);
    g.setColour (juce::Colour (0xff1b1a20));
    g.fillRect (0.0f, stripY, w, stripH);
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int midi = 12; midi < 72; ++midi)
        {
            const float f0 = 440.0f * std::pow (2.0f, ((float) midi - 69.5f) / 12.0f);
            const float f1 = 440.0f * std::pow (2.0f, ((float) midi - 68.5f) / 12.0f);
            const float x0 = xForFreq (juce::jmax (20.0f, f0), w), x1 = xForFreq (f1, w);
            if (x1 <= 0.0f) continue;
            const int note = midi % 12;
            const bool black = note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
            auto keyRect = juce::Rectangle<float> (x0, stripY + 3.0f, x1 - x0, stripH - 6.0f);

            if (pass == 0)
            {
                juce::Colour fillCol = black ? juce::Colour (0xff2a2930) : juce::Colour (0xff6e6b78);
                if (midi == keyMidi) fillCol = palette::butter;
                g.setColour (fillCol);
                g.fillRect (keyRect.reduced (0.5f, black ? 4.0f : 0.0f));
            }
            else if (note == 0 || midi == keyMidi)
            {
                const auto txt = params::noteNames[note] + juce::String (midi / 12 - 1);
                const bool isKey = midi == keyMidi;
                auto pill = juce::Rectangle<float> (isKey ? keyRect.getCentreX() - 14.0f : x0 + 1.0f, stripY + 8.0f, 28.0f, 14.0f);
                g.setColour (isKey ? palette::butter : juce::Colour (0xff1b1a20).withAlpha (0.85f));
                g.fillRoundedRectangle (pill, 4.0f);
                g.setColour (isKey ? palette::ink : palette::text);
                g.setFont (bold (10.0f));
                g.drawText (txt, pill.toNearestInt(), juce::Justification::centred, false);
            }
        }
    }

    // Étiquette de la note KEY sur sa ligne
    auto keyTag = juce::Rectangle<float> (kx - 16.0f, h - 22.0f, 32.0f, 16.0f);
    g.setColour (palette::butter);
    g.fillRoundedRectangle (keyTag, 4.0f);
    g.setColour (palette::ink);
    g.setFont (bold (10.5f));
    g.drawText (params::noteNameForFrequency (keyHz), keyTag.toNearestInt(), juce::Justification::centred);

    g.setColour (palette::edge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.4f);
}


// ----------------------------------------------------------------------------
//  Calques : chaque module dessine son action dans sa couleur
// ----------------------------------------------------------------------------
namespace
{
    // Réponse d'un passe-bande normalisé (module complexe)
    std::complex<double> bandpass (double f, double f0, double q)
    {
        const double r = f / f0;
        return 1.0 / std::complex<double> (1.0, q * (r - 1.0 / r));
    }
    // Poids de la bande grave (Linkwitz-Riley 4e ordre)
    double lowWeight (double f, double fc) { const double r = f / fc; return 1.0 / (1.0 + r * r * r * r); }
}

void SpectrumView::drawOverlays (juce::Graphics& g, float w, float h)
{
    auto val = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    auto on  = [&val] (const char* id) { return val (id) > 0.5f; };

    const float refY = h * 0.42f;          // ligne 0 dB des courbes de réglage
    const float pxPerDb = 4.2f;
    const double fc = val ("crossover");
    const double key = proc.getKeyFrequency();
    const float played = playedFrequency ? playedFrequency() : 0.0f;
    const double root = played > 20.0f ? played : key;
    const float xx = xForFreq ((float) fc, w);

    struct Layer { const char* id; juce::Colour col; bool enabled; };
    const Layer layers[] {
        { "gen",   palette::sky,      on ("genOn") },
        { "tone",  palette::lavender, on ("toneOn") },
        { "drive", palette::peach,    on ("driveOn") },
        { "shape", palette::butter,   on ("shapeOn") },
        { "pump",  palette::rose,     on ("pumpOn") },
        { "width", palette::mint,     on ("widthOn") },
    };

    auto alphaFor = [this] (const juce::String& id, bool enabled)
    {
        const float f = focusId == id ? focusAlpha : 0.0f;
        return juce::jmax (enabled ? 0.38f : 0.0f, f * (enabled ? 1.0f : 0.55f));
    };
    auto thickness = [this] (const juce::String& id) { return 1.3f + (focusId == id ? focusAlpha * 1.6f : 0.0f); };

    // Trace une courbe de gain (dB) sur toute la largeur
    auto curve = [&] (const std::function<double (double)>& gainDb, juce::Colour col, float alpha, float thick, bool fill)
    {
        juce::Path p;
        for (int px = 0; px <= (int) w; px += 2)
        {
            const double f = 20.0 * std::pow (1000.0, px / (double) w);
            const float y = juce::jlimit (4.0f, h - 4.0f, refY - (float) gainDb (f) * pxPerDb);
            if (px == 0) p.startNewSubPath ((float) px, y); else p.lineTo ((float) px, y);
        }
        if (fill)
        {
            juce::Path f (p);
            f.lineTo (w, refY);
            f.lineTo (0.0f, refY);
            f.closeSubPath();
            g.setColour (col.withAlpha (alpha * 0.18f));
            g.fillPath (f);
        }
        g.setColour (col.withAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    // Ligne de référence quand un réglage est mis en avant
    if (focusAlpha > 0.0f && focusId.isNotEmpty())
    {
        g.setColour (palette::text.withAlpha (0.12f * focusAlpha));
        g.drawHorizontalLine ((int) refY, 0.0f, w);
    }

    for (auto& L : layers)
    {
        const float a = alphaFor (L.id, L.enabled);
        if (a <= 0.01f) continue;
        const float t = thickness (L.id);
        const juce::String id (L.id);

        if (id == "gen")
        {
            const bool sine = val ("genType") > 0.5f;
            const double level = val ("genLevel") * 0.01, tone = val ("genTone") * 0.01;
            const double f0 = sine ? (on ("keyLock") ? key : root) : root * 0.5;
            curve ([=] (double f)
            {
                const double lw = lowWeight (f, fc);
                double lin = sine ? (1.0 - tone * lw) + level * 2.0 * std::abs (bandpass (f, f0, 6.0)) * lw
                                  : 1.0 + level * 2.5 * std::abs (bandpass (f, f0, 3.0 + tone * 4.0)) * lw;
                return 20.0 * std::log10 (juce::jmax (0.05, lin));
            }, L.col, a, t, true);
            const float x0 = xForFreq ((float) f0, w);
            g.setColour (L.col.withAlpha (a));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x0, refY - 10.0f }));
        }
        else if (id == "tone")
        {
            const double f0 = juce::jmin (key * (val ("toneHarm") + 1.0), 18000.0);
            const double q = 1.0 + val ("toneQ") * 0.15;
            const double k = val ("toneAmt") * 0.02;
            curve ([=] (double f)
            {
                const double G = std::abs (1.0 + k * bandpass (f, f0, q));
                const double lin = 1.0 + lowWeight (f, fc) * (G - 1.0);
                return 20.0 * std::log10 (lin);
            }, L.col, a, t, true);
        }
        else if (id == "drive")
        {
            const double drive = val ("drive") * 0.01, color = val ("color") * 0.01, focus = val ("focus") * 0.01;
            if (focus > 0.0)
                curve ([=] (double f) { return 20.0 * std::log10 (std::abs (1.0 + focus * 2.0 * bandpass (f, root, 2.0)) * lowWeight (f, fc) + (1.0 - lowWeight (f, fc))); },
                       L.col, a * 0.8f, t, false);
            // Peigne d'harmoniques générées
            g.setColour (L.col.withAlpha (a));
            for (int n = 2; n <= 10; ++n)
            {
                const double fn = root * n;
                if (fn > 20000.0) break;
                double db = drive * (20.0 - 1.8 * n);
                if (n % 2 == 0) db += color * 8.0;
                if (db <= 0.5) continue;
                const float x = xForFreq ((float) fn, w);
                const float y = refY - (float) db * pxPerDb;
                g.drawLine (x, refY, x, y, t);
                g.fillEllipse (juce::Rectangle<float> (t * 3.2f, t * 3.2f).withCentre ({ x, y }));
                if (focusId == id && focusAlpha > 0.3f)
                {
                    g.setFont (bold (9.0f));
                    g.drawText (juce::String (n) + "x", juce::Rectangle<float> (24.0f, 11.0f).withCentre ({ x, y - 9.0f }), juce::Justification::centred);
                }
            }
        }
        else if (id == "shape")
        {
            // Enveloppe schématique dans la zone grave
            const double att = val ("attack") * 0.01, sus = val ("sustain") * 0.01;
            const float x0 = 16.0f, x1 = juce::jmax (x0 + 60.0f, xx - 10.0f);
            const float base = h - 12.0f, top = h * 0.55f;
            const float span = x1 - x0;
            const float peak = base - (base - top) * (float) (0.6 + 0.4 * att);
            const float sustainY = base - (base - top) * (float) (0.35 + 0.25 * sus);
            juce::Path env;
            env.startNewSubPath (x0, base);
            env.lineTo (x0 + span * 0.05f, peak);
            env.quadraticTo (x0 + span * 0.12f, sustainY, x0 + span * 0.2f, sustainY);
            env.lineTo (x0 + span * (float) (0.45 + 0.3 * sus), sustainY);
            env.quadraticTo (x0 + span * (float) (0.6 + 0.3 * sus), base, x0 + span * (float) (0.75 + 0.2 * sus), base);
            g.setColour (L.col.withAlpha (a * 0.15f));
            g.fillPath (env);
            g.setColour (L.col.withAlpha (a));
            g.strokePath (env, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (id == "pump")
        {
            // Cycles de ducking + niveau courant
            const float depth = val ("pumpDepth") * 0.01f;
            const float release = 0.15f + val ("pumpShape") * 0.01f * 0.75f;
            const float x0 = 16.0f, x1 = juce::jmax (x0 + 60.0f, xx - 16.0f);
            const float base = h - 10.0f, height = h * 0.22f;
            juce::Path p;
            const int cycles = 4;
            for (int i = 0; i <= 200; ++i)
            {
                const float ph = std::fmod ((float) i / 200.0f * (float) cycles, 1.0f);
                const float tt = juce::jmin (1.0f, ph / release);
                const float c = std::sin (tt * juce::MathConstants<float>::halfPi);
                const float gain = 1.0f - depth * (1.0f - c * c);
                const float x = x0 + (x1 - x0) * (float) i / 200.0f;
                const float y = base - height * gain;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (L.col.withAlpha (a));
            g.strokePath (p, juce::PathStrokeType (t));
            const float now = proc.pumpGainNow.load();
            auto bar = juce::Rectangle<float> (x1 + 4.0f, base - height * now, 5.0f, height * now);
            g.fillRoundedRectangle (bar, 2.0f);
        }
        else if (id == "width")
        {
            const float wv = val ("width") * 0.01f;
            g.setColour (L.col.withAlpha (a * 0.07f * juce::jmax (0.3f, wv)));
            g.fillRect (xx, 0.0f, w - xx, h);
            const float cy = h * 0.2f, cx = xx + (w - xx) * 0.5f;
            const float half = (w - xx) * 0.08f * (0.4f + wv);
            juce::Path arrow;
            arrow.addArrow ({ cx, cy, cx - half, cy }, t, 9.0f, 8.0f);
            arrow.addArrow ({ cx, cy, cx + half, cy }, t, 9.0f, 8.0f);
            g.setColour (L.col.withAlpha (a));
            g.fillPath (arrow);
        }
    }

    // Réglages globaux (uniquement quand ils sont touchés)
    if (focusAlpha > 0.01f)
    {
        const float a = focusAlpha;
        if (focusId == "crossover")
        {
            curve ([=] (double f) { return 20.0 * std::log10 (juce::jmax (1.0e-3, lowWeight (f, fc))); }, palette::aqua, a, 2.2f, true);
            curve ([=] (double f) { const double r = f / fc; return 20.0 * std::log10 (juce::jmax (1.0e-3, (r * r * r * r) / (1.0 + r * r * r * r))); },
                   palette::aqua.withMultipliedBrightness (0.8f), a * 0.7f, 1.4f, false);
        }
        else if (focusId == "input" || focusId == "output")
        {
            const float db = val (focusId == "input" ? "inGain" : "output");
            const float y = juce::jlimit (4.0f, h - 4.0f, refY - db * pxPerDb);
            g.setColour (palette::aqua.withAlpha (a));
            g.drawLine (0.0f, y, w, y, 2.0f);
        }
        else if (focusId == "mix")
        {
            const double m = val ("mix") * 0.01;
            curve ([=] (double f) { return 20.0 * std::log10 (1.0 - (1.0 - m) * 0.85 * lowWeight (f, fc)); }, palette::aqua, a, 2.0f, true);
        }
        else if (focusId == "key")
        {
            g.setColour (palette::butter.withAlpha (a));
            for (int n = 1; n <= 4; ++n)
            {
                const float x = xForFreq ((float) key * (float) n, w);
                g.drawLine (x, 0.0f, x, h, n == 1 ? 2.5f : 1.2f);
            }
        }
    }

    // Étiquette du réglage mis en avant
    if (focusAlpha > 0.05f && focusId.isNotEmpty())
    {
        const std::map<juce::String, std::pair<juce::String, juce::Colour>> names {
            { "gen", { "GEN", palette::sky } }, { "tone", { "TONE", palette::lavender } },
            { "drive", { "DRIVE", palette::peach } }, { "shape", { "SHAPE", palette::butter } },
            { "pump", { "PUMP", palette::rose } }, { "width", { "WIDTH", palette::mint } },
            { "crossover", { "CROSSOVER", palette::aqua } }, { "input", { "INPUT", palette::aqua } },
            { "output", { "OUTPUT", palette::aqua } }, { "mix", { "MIX", palette::aqua } },
            { "key", { "KEY", palette::butter } },
        };
        auto it = names.find (focusId);
        if (it != names.end())
        {
            juce::String detail;
            if (focusId == "tone")
            {
                const float f0 = (float) key * (val ("toneHarm") + 1.0f);
                detail = params::noteNameForFrequency (f0) + "  " + juce::String (f0, 1) + " Hz";
            }
            else if (focusId == "gen")
                detail = val ("genType") > 0.5f ? (on ("keyLock") ? "SINE on " + params::noteNameForFrequency ((float) key) : juce::String ("SINE tracking"))
                                                 : "OCTAVE -12";
            else if (focusId == "drive")
                detail = juce::StringArray { "TAPE", "TUBE", "HARD", "FOLD" }[(int) val ("driveType")] + "  " + juce::String ((int) val ("drive")) + " %";
            else if (focusId == "pump")
                detail = juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" }[(int) val ("pumpRate")] + "  depth " + juce::String ((int) val ("pumpDepth")) + " %";
            else if (focusId == "crossover") detail = juce::String ((int) fc) + " Hz";
            else if (focusId == "key")       detail = params::noteNameForFrequency ((float) key) + "  " + juce::String (key, 1) + " Hz";
            else if (focusId == "input")     detail = juce::String (val ("inGain"), 1) + " dB";
            else if (focusId == "output")    detail = juce::String (val ("output"), 1) + " dB";
            else if (focusId == "mix")       detail = juce::String ((int) val ("mix")) + " %";
            else if (focusId == "shape")     detail = "ATT " + juce::String ((int) val ("attack")) + "  SUS " + juce::String ((int) val ("sustain"));
            else if (focusId == "width")     detail = juce::String ((int) val ("width")) + " %";

            const auto text = it->second.first + (detail.isNotEmpty() ? "   " + detail : juce::String());
            const auto font = bold (12.0f);
            const float tw = juce::GlyphArrangement::getStringWidth (font, text) + 20.0f;
            auto pill = juce::Rectangle<float> (w - tw - 10.0f, 20.0f, tw, 20.0f);
            g.setColour (it->second.second.withAlpha (focusAlpha));
            g.fillRoundedRectangle (pill, 10.0f);
            g.setColour (palette::ink.withAlpha (focusAlpha));
            g.setFont (font);
            g.drawText (text, pill.toNearestInt(), juce::Justification::centred);
        }
    }
}

// ============================================================================
//  Accordeur (YIN)
// ============================================================================
TunerView::TunerView (SubShaperProcessor& p) : proc (p)
{
    scratch.resize ((size_t) SubShaperProcessor::fifoSize);
    diff.resize ((size_t) bufSize / 2);
    startTimerHz (12);
}

float TunerView::detect (double fs)
{
    const int half = bufSize / 2;
    double energy = 0.0;
    for (auto v : ring) energy += (double) v * v;
    if (energy / bufSize < 1.0e-6)
        return 0.0f;

    const int tauMin = juce::jmax (2, (int) (fs / 400.0));
    const int tauMax = juce::jmin (half - 1, (int) (fs / 25.0));

    diff[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        for (int i = 0; i < half; ++i)
        {
            const double d = (double) ring[(size_t) i] - (double) ring[(size_t) (i + tau)];
            sum += d * d;
        }
        running += sum;
        diff[(size_t) tau] = running > 0.0 ? (float) (sum * tau / running) : 1.0f;
    }

    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (diff[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
        return 0.0f;

    // Interpolation parabolique
    const float a = diff[(size_t) best - 1], b0 = diff[(size_t) best], c = diff[(size_t) best + 1];
    const float denom = a - 2.0f * b0 + c;
    const float shift = std::abs (denom) > 1.0e-9f ? 0.5f * (a - c) / denom : 0.0f;
    return (float) (fs / ((double) best + shift));
}

void TunerView::timerCallback()
{
    auto& fifo = proc.tunerFifo;
    const int available = fifo.getNumReady();
    if (available > 0)
    {
        const auto scope = fifo.read (available);
        int n = 0;
        for (int k = 0; k < scope.blockSize1; ++k) scratch[(size_t) n++] = proc.tunerData[(size_t) (scope.startIndex1 + k)];
        for (int k = 0; k < scope.blockSize2; ++k) scratch[(size_t) n++] = proc.tunerData[(size_t) (scope.startIndex2 + k)];
        if (n >= bufSize)
            std::copy (scratch.begin() + (n - bufSize), scratch.begin() + n, ring.begin());
        else
        {
            std::move (ring.begin() + n, ring.end(), ring.begin());
            std::copy (scratch.begin(), scratch.begin() + n, ring.end() - n);
        }
    }

    const float f = detect (proc.currentSampleRate.load() / 4.0);
    if (f > 0.0f)
    {
        freq = f;
        shownFreq = shownFreq > 0.0f && std::abs (f / shownFreq - 1.0f) < 0.03f ? shownFreq + 0.4f * (f - shownFreq) : f;
        holdFrames = 10;
    }
    else if (holdFrames > 0 && --holdFrames == 0)
    {
        shownFreq = 0.0f;
    }
    repaint();
}

void TunerView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (palette::screen);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (palette::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.1f);

    auto inner = r.reduced (10.0f, 6.0f);
    g.setColour (palette::textDim);
    g.setFont (bold (10.5f));
    g.drawText ("TUNER", inner.removeFromTop (12.0f), juce::Justification::centredLeft);

    auto noteArea = inner.removeFromLeft (92.0f);
    float cents = 0.0f;
    const bool active = shownFreq > 0.0f;
    const auto name = active ? params::noteNameForFrequency (shownFreq, &cents) : juce::String ("--");
    const bool inTune = active && std::abs (cents) < 6.0f;
    g.setColour (active ? (inTune ? palette::mint : palette::peach) : palette::textDim);
    g.setFont (bold (30.0f));
    g.drawText (name, noteArea.toNearestInt(), juce::Justification::centredLeft);

    // Barre de cents
    auto bar = inner.withTrimmedTop (4.0f).removeFromTop (14.0f);
    g.setColour (juce::Colour (0xff2c2b33));
    g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (palette::textDim);
    g.drawVerticalLine ((int) bar.getCentreX(), bar.getY() - 2.0f, bar.getBottom() + 2.0f);
    if (active)
    {
        const float x = bar.getCentreX() + juce::jlimit (-50.0f, 50.0f, cents) / 50.0f * (bar.getWidth() * 0.5f - 4.0f);
        g.setColour (inTune ? palette::mint : palette::peach);
        g.fillRoundedRectangle (juce::Rectangle<float> (7.0f, 18.0f).withCentre ({ x, bar.getCentreY() }), 3.0f);
    }
    g.setFont (plain (11.5f));
    g.setColour (palette::text);
    const auto info = active ? juce::String (shownFreq, 1) + " Hz   " + (cents >= 0 ? "+" : "") + juce::String ((int) std::round (cents)) + " ct"
                             : juce::String ("play a bass note");
    g.drawText (info, inner.withTrimmedTop (22.0f).toNearestInt(), juce::Justification::centredLeft);
}

// ============================================================================
//  Vumètre
// ============================================================================
void MeterView::timerCallback()
{
    const float l = proc.meterL.exchange (0.0f), r = proc.meterR.exchange (0.0f);
    levelL = juce::jmax (l, levelL * 0.85f);
    levelR = juce::jmax (r, levelR * 0.85f);
    if (l >= 0.999f || r >= 0.999f) clipHold = 45;
    else if (clipHold > 0) --clipHold;
    repaint();
}

void MeterView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto clip = r.removeFromTop (12.0f);
    g.setColour (clipHold > 0 ? palette::coral : juce::Colour (0xff3a2f33));
    g.fillRoundedRectangle (clip.reduced (2.0f, 1.0f), 3.0f);
    r.removeFromTop (4.0f);

    auto drawBar = [&] (juce::Rectangle<float> b, float level)
    {
        g.setColour (palette::screen);
        g.fillRoundedRectangle (b, 3.0f);
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = b.withTrimmedTop (b.getHeight() * (1.0f - norm));
        g.setGradientFill (juce::ColourGradient (palette::coral, 0.0f, b.getY(), palette::mint, 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (fill, 3.0f);
    };
    const float bw = (r.getWidth() - 4.0f) * 0.5f;
    drawBar (r.removeFromLeft (bw), levelL);
    r.removeFromLeft (4.0f);
    drawBar (r, levelR);
}

// ============================================================================
//  Potard, sélecteur segmenté
// ============================================================================
LabelledKnob::LabelledKnob()
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, palette::text);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (bold (11.5f));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void LabelledKnob::setAccentColour (juce::Colour c)
{
    setAccent (slider, c);
    label.setColour (juce::Label::textColourId, c);
}

void LabelledKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (15));
    slider.setBounds (r);
}

ChoiceSegments::ChoiceSegments (juce::RangedAudioParameter& param, const juce::StringArray& labels,
                                juce::Colour accent, bool vert, juce::UndoManager* um)
    : vertical (vert)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->getProperties().set ("segment", true);
        setAccent (*b, accent);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        b->onClick = [this, i] { if (attachment) attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v)
    {
        const int idx = juce::roundToInt (v);
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
    }, um);
    attachment->sendInitialUpdate();
}

void ChoiceSegments::resized()
{
    auto r = getLocalBounds();
    const int n = buttons.size();
    const int gap = 4;
    for (int i = 0; i < n; ++i)
    {
        if (vertical)
        {
            const int h = (r.getHeight() - gap * (n - 1 - i)) / (n - i);
            buttons[i]->setBounds (r.removeFromTop (h));
            r.removeFromTop (gap);
        }
        else
        {
            const int w = (r.getWidth() - gap * (n - 1 - i)) / (n - i);
            buttons[i]->setBounds (r.removeFromLeft (w));
            r.removeFromLeft (gap);
        }
    }
}

// ============================================================================
//  Panneau principal
// ============================================================================
static const juce::StringArray noteLabels { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

Panel::Panel (SubShaperProcessor& p)
    : proc (p), spectrum (p), tuner (p), meter (p)
{
    setLookAndFeel (&lnf);
    auto* um = &proc.undoManager;
    auto& ap = proc.apvts;

    addAndMakeVisible (spectrum);
    addAndMakeVisible (tuner);
    addAndMakeVisible (meter);
    tuner.setTooltip ("Detects the note played by your bass (monophonic). Green = in tune.");

    // --- Barre du haut ---
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &slotA, &slotB, &copyBtn, &undoBtn, &redoBtn })
    {
        setAccent (*b, palette::sky);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (b);
    }
    slotA.getProperties().set ("segment", true);
    slotB.getProperties().set ("segment", true);
    setAccent (slotA, palette::peach);
    setAccent (slotB, palette::peach);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePresetDialog(); };
    slotA.onClick = [this] { proc.selectSlot (0); };
    slotB.onClick = [this] { proc.selectSlot (1); };
    copyBtn.onClick = [this] { proc.copyToOtherSlot(); };
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    prevBtn.setTooltip ("Previous preset");
    nextBtn.setTooltip ("Next preset");
    saveBtn.setTooltip ("Save current settings as a user preset");
    slotA.setTooltip ("Compare: settings A");
    slotB.setTooltip ("Compare: settings B");
    copyBtn.setTooltip ("Copy current settings to the other slot");
    undoBtn.setTooltip ("Undo last change");
    redoBtn.setTooltip ("Redo");

    presetBox.setTooltip ("Factory and user presets");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        proc.undoManager.beginNewTransaction();
        if (id < 1000) proc.loadFactoryPreset (id - 1);
        else if (juce::isPositiveAndBelow (id - 1000, userFiles.size())) proc.loadUserPreset (userFiles[id - 1000]);
    };
    addAndMakeVisible (presetBox);

    scaleBox.addItem ("100%", 1);
    scaleBox.addItem ("125%", 2);
    scaleBox.addItem ("150%", 3);
    const float sc = (float) (double) ap.state.getProperty ("uiScale", 1.0);
    scaleBox.setSelectedId (sc > 1.4f ? 3 : (sc > 1.1f ? 2 : 1), juce::dontSendNotification);
    scaleBox.setTooltip ("Window size");
    scaleBox.onChange = [this]
    {
        const float s = scaleBox.getSelectedId() == 3 ? 1.5f : (scaleBox.getSelectedId() == 2 ? 1.25f : 1.0f);
        proc.apvts.state.setProperty ("uiScale", s, nullptr);
        if (onScaleChange) onScaleChange (s);
    };
    addAndMakeVisible (scaleBox);

    // --- KEY ---
    keyKnob.slider.getProperties().set ("steps", noteLabels.joinIntoString ("|"));
    keyKnob.slider.setRotaryParameters (juce::degreesToRadians (195.0f), juce::degreesToRadians (525.0f), true);
    keyKnob.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    attachKnob (keyKnob, "keyNote", "", palette::butter,
                "KEY: root note of your track. Tunes TONE, DRIVE focus and SINE key lock. Snaps to notes.");
    octaveSeg = std::make_unique<ChoiceSegments> (*ap.getParameter ("keyOct"),
                    juce::StringArray { "OCT 0", "OCT 1", "OCT 2", "OCT 3" }, palette::butter, true, um);
    octaveSeg->setTooltip ("Octave of the key note (OCT 1 = typical 808 range)");
    addAndMakeVisible (*octaveSeg);
    keyReadout.setJustificationType (juce::Justification::centred);
    keyReadout.setFont (bold (13.0f));
    keyReadout.setColour (juce::Label::textColourId, palette::butter);
    addAndMakeVisible (keyReadout);

    // --- Modules ---
    modules = {
        { "GEN",   palette::sky,      {} },
        { "TONE",  palette::lavender, {} },
        { "DRIVE", palette::peach,    {} },
        { "SHAPE", palette::butter,   {} },
        { "PUMP",  palette::rose,     {} },
        { "WIDTH", palette::mint,     {} },
    };

    attachToggle (genOn,   "genOn",   "GEN",   palette::sky,      "Sub generator: adds an octave-down sub or a clean sine");
    attachToggle (toneOn,  "toneOn",  "TONE",  palette::lavender, "Resonant boost tuned to the KEY note");
    attachToggle (driveOn, "driveOn", "DRIVE", palette::peach,    "Saturation so the bass translates on small speakers");
    attachToggle (shapeOn, "shapeOn", "SHAPE", palette::butter,   "Attack / sustain control of the low end (808 tail)");
    attachToggle (pumpOn,  "pumpOn",  "PUMP",  palette::rose,     "Tempo-synced ducking of the low end (sidechain style)");
    attachToggle (widthOn, "widthOn", "WIDTH", palette::mint,     "Stereo width above the crossover only; sub stays mono");
    for (auto* t : { &genOn, &toneOn, &driveOn, &shapeOn, &pumpOn, &widthOn })
        t->getProperties().set ("fontSize", 15.0f);

    genTypeSeg = std::make_unique<ChoiceSegments> (*ap.getParameter ("genType"), juce::StringArray { "OCTAVE", "SINE" },
                                                   palette::sky, false, um);
    driveTypeSeg = std::make_unique<ChoiceSegments> (*ap.getParameter ("driveType"),
                                                     juce::StringArray { "TAPE", "TUBE", "HARD", "FOLD" }, palette::peach, false, um);
    pumpRateSeg = std::make_unique<ChoiceSegments> (*ap.getParameter ("pumpRate"),
                                                    juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" }, palette::rose, false, um);
    genTypeSeg->setTooltip ("OCTAVE: sub one octave below. SINE: pure sine following the pitch (or the KEY)");
    driveTypeSeg->setTooltip ("Saturation flavour");
    pumpRateSeg->setTooltip ("Pump rate, synced to the host tempo");
    for (auto* s : { genTypeSeg.get(), driveTypeSeg.get(), pumpRateSeg.get() })
        addAndMakeVisible (s);

    attachKnob (genLevel, "genLevel", "LEVEL", palette::sky, "Level of the generated sub");
    attachKnob (genTone, "genTone", "TONE", palette::sky, "OCTAVE: sub brightness. SINE: how much the original low end is replaced");
    attachToggle (keyLockBtn, "keyLock", "KEY LOCK", palette::butter, "SINE mode: lock the sine to the KEY note instead of tracking the pitch");

    attachKnob (toneAmt, "toneAmt", "AMOUNT", palette::lavender, "Boost amount at the tuned frequency");
    attachKnob (toneQ, "toneQ", "Q", palette::lavender, "Narrowness of the boost");
    attachKnob (toneHarm, "toneHarm", "HARMONIC", palette::lavender, "Which harmonic of the KEY is boosted");
    toneHarm.slider.getProperties().set ("steps", "1x|2x|3x|4x");
    toneHarm.slider.setRotaryParameters (juce::degreesToRadians (240.0f), juce::degreesToRadians (480.0f), true);
    toneHarm.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    attachKnob (drive, "drive", "DRIVE", palette::peach, "Saturation amount (level is compensated)");
    attachKnob (color, "color", "COLOR", palette::peach, "Asymmetry: adds even harmonics / grit");
    attachKnob (focus, "focus", "FOCUS", palette::peach, "Emphasises the KEY note before saturation so harmonics follow the root");
    attachKnob (driveMix, "driveMix", "MIX", palette::peach, "Parallel blend of the saturation");

    attachKnob (attack, "attack", "ATTACK", palette::butter, "Punch of the low end (negative = softer)");
    attachKnob (sustain, "sustain", "SUSTAIN", palette::butter, "Tail length of the low end (negative = tighter 808)");

    attachKnob (pumpDepth, "pumpDepth", "DEPTH", palette::rose, "How much the low end ducks on each beat");
    attachKnob (pumpShape, "pumpShape", "RELEASE", palette::rose, "How long the low end takes to come back");

    attachKnob (width, "width", "WIDTH", palette::mint, "Stereo width above the crossover (100% = unchanged)");

    for (auto* l : { &toneFreqLabel, &shapeInfo, &pumpInfo, &widthInfo })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (plain (11.5f));
        l->setColour (juce::Label::textColourId, palette::textDim);
        addAndMakeVisible (l);
    }
    toneFreqLabel.setColour (juce::Label::textColourId, palette::lavender);
    shapeInfo.setText ("Tighten or lengthen\nthe 808 tail", juce::dontSendNotification);
    widthInfo.setText ("Above crossover only.\nSub stays mono.", juce::dontSendNotification);

    // --- Global ---
    attachKnob (inGain, "inGain", "INPUT", palette::aqua, "Input level: drives the whole chain harder or softer");
    attachKnob (crossover, "crossover", "CROSSOVER", palette::aqua, "Split frequency: everything below is processed");
    attachKnob (mix, "mix", "MIX", palette::aqua, "Global dry / wet of the low-end processing");
    attachKnob (output, "output", "OUTPUT", palette::aqua, "Output level");

    attachToggle (monoLow, "monoLow", "MONO LOW", palette::butter, "Sums the low end to mono");
    attachToggle (soloLow, "soloLow", "SOLO LOW", palette::coral, "Listen to the low band only");
    attachToggle (subCut, "subCut", "SUB CUT 25", palette::aqua, "Removes rumble below 25 Hz");
    attachToggle (delta, "delta", "DELTA", palette::rose, "Listen only to what Subshaper adds or removes");
    attachToggle (gainMatch, "gainMatch", "GAIN MATCH", palette::mint, "Keeps the output as loud as the input for fair comparisons");
    attachToggle (hq, "hq", "HQ (x4)", palette::lavender, "On: 4x oversampling (best). Off: 2x (lower CPU)");

    // Formats d'affichage
    crossover.slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + " Hz"; };
    for (auto* k : { &inGain, &output })
        k->slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    for (auto* k : { &genLevel, &genTone, &toneAmt, &toneQ, &drive, &color, &focus, &driveMix, &pumpDepth, &pumpShape, &width, &mix })
        k->slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + " %"; };
    for (auto* k : { &attack, &sustain })
        k->slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) std::round (v)) + " %"; };
    for (auto* k : { &crossover, &inGain, &output, &genLevel, &genTone, &toneAmt, &toneQ, &drive, &color, &focus,
                     &driveMix, &pumpDepth, &pumpShape, &width, &mix, &attack, &sustain })
        k->slider.updateText();

    // Survol / réglage -> mise en avant sur l'écran
    for (auto* c : std::initializer_list<juce::Component*> { &genOn, genTypeSeg.get(), &genLevel, &genTone, &keyLockBtn })
        registerFocus (*c, "gen");
    for (auto* c : std::initializer_list<juce::Component*> { &toneOn, &toneAmt, &toneQ, &toneHarm })
        registerFocus (*c, "tone");
    for (auto* c : std::initializer_list<juce::Component*> { &driveOn, driveTypeSeg.get(), &drive, &color, &focus, &driveMix })
        registerFocus (*c, "drive");
    for (auto* c : std::initializer_list<juce::Component*> { &shapeOn, &attack, &sustain })
        registerFocus (*c, "shape");
    for (auto* c : std::initializer_list<juce::Component*> { &pumpOn, pumpRateSeg.get(), &pumpDepth, &pumpShape })
        registerFocus (*c, "pump");
    for (auto* c : std::initializer_list<juce::Component*> { &widthOn, &width })
        registerFocus (*c, "width");
    registerFocus (crossover, "crossover");
    registerFocus (inGain, "input");
    registerFocus (output, "output");
    registerFocus (mix, "mix");
    registerFocus (keyKnob, "key");
    registerFocus (*octaveSeg, "key");
    spectrum.playedFrequency = [this] { return tuner.getFrequency(); };

    for (auto* prm : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            lastValues[rp->getParameterID()] = rp->getValue();

    refreshPresetBox();
    setSize (baseW, baseH);
    startTimerHz (20);
    timerCallback();
}

Panel::~Panel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Panel::registerFocus (juce::Component& c, const juce::String& id)
{
    focusRelay.ids[&c] = id;
    c.addMouseListener (&focusRelay, true);
}

void Panel::FocusRelay::update (const juce::MouseEvent& e, bool hold)
{
    for (auto* c = e.eventComponent; c != nullptr && c != &panel; c = c->getParentComponent())
    {
        auto it = ids.find (c);
        if (it != ids.end())
        {
            panel.spectrum.setFocus (it->second, hold);
            return;
        }
    }
}

juce::String Panel::moduleForParam (const juce::String& id)
{
    if (id.startsWith ("gen") || id == "keyLock") return "gen";
    if (id.startsWith ("tone")) return "tone";
    if (id.startsWith ("drive") || id == "color" || id == "focus") return "drive";
    if (id == "shapeOn" || id == "attack" || id == "sustain") return "shape";
    if (id.startsWith ("pump")) return "pump";
    if (id.startsWith ("width")) return "width";
    if (id == "crossover") return "crossover";
    if (id == "inGain") return "input";
    if (id == "output") return "output";
    if (id == "mix") return "mix";
    if (id.startsWith ("key")) return "key";
    return {};
}

void Panel::attachKnob (LabelledKnob& k, const juce::String& id, const juce::String& text, juce::Colour c, const juce::String& tip)
{
    k.label.setText (text, juce::dontSendNotification);
    k.setAccentColour (c);
    k.slider.setTooltip (tip);
    k.slider.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
    addAndMakeVisible (k);
    sliderAtts.add (new SliderAtt (proc.apvts, id, k.slider));
}

void Panel::attachToggle (juce::ToggleButton& t, const juce::String& id, const juce::String& text, juce::Colour c, const juce::String& tip)
{
    t.setButtonText (text);
    setAccent (t, c);
    t.setTooltip (tip);
    t.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    t.onClick = [this] { proc.undoManager.beginNewTransaction(); };
    addAndMakeVisible (t);
    buttonAtts.add (new ButtonAtt (proc.apvts, id, t));
}

void Panel::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& list = presets::factory();
    juce::String category;
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (category != list[i].category)
        {
            category = list[i].category;
            if (category.isNotEmpty())
                presetBox.addSectionHeading (category);
        }
        presetBox.addItem (list[i].name, (int) i + 1);
    }
    userFiles = presets::userPresets();
    if (! userFiles.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userFiles.size(); ++i)
            presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), 1000 + i);
    }
    lastPresetName.clear();
}

void Panel::stepPreset (int step)
{
    const int count = presetBox.getNumItems();
    if (count == 0) return;
    int index = -1;
    for (int i = 0; i < count; ++i)
        if (presetBox.getItemId (i) == presetBox.getSelectedId()) index = i;
    index = (index + step + count) % count;
    presetBox.setSelectedId (presetBox.getItemId (index), juce::sendNotificationSync);
}

void Panel::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", proc.getPresetName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Panel> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            const auto name = w->getTextEditorContents ("name");
            if (safe->proc.saveUserPreset (name))
            {
                safe->refreshPresetBox();
                safe->timerCallback();
            }
        }
    }), true);
}

void Panel::timerCallback()
{
    {
        juce::String changedModule;
        int changes = 0;
        for (auto& [id, last] : lastValues)
        {
            const float now = proc.apvts.getParameter (id)->getValue();
            if (std::abs (now - last) > 1.0e-4f)
            {
                last = now;
                const auto m = moduleForParam (id);
                if (m.isNotEmpty()) { changedModule = m; ++changes; }
            }
        }
        if (changes >= 1 && changes <= 2)   // pas de flash lors d'un chargement de preset
            spectrum.flash (changedModule);
    }

    // Nom du preset
    const auto name = proc.getPresetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == name) found = presetBox.getItemId (i);
        if (found > 0) presetBox.setSelectedId (found, juce::dontSendNotification);
        else presetBox.setText (name, juce::dontSendNotification);
    }

    slotA.setToggleState (proc.getActiveSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState (proc.getActiveSlot() == 1, juce::dontSendNotification);
    copyBtn.setButtonText (proc.getActiveSlot() == 0 ? "A > B" : "B > A");
    undoBtn.setEnabled (proc.undoManager.canUndo());
    redoBtn.setEnabled (proc.undoManager.canRedo());

    // KEY
    const float keyHz = proc.getKeyFrequency();
    keyReadout.setText (params::noteNameForFrequency (keyHz) + "  " + juce::String (keyHz, 1) + " Hz", juce::dontSendNotification);
    const int harm = (int) proc.apvts.getRawParameterValue ("toneHarm")->load() + 1;
    const float toneHz = keyHz * (float) harm;
    toneFreqLabel.setText (params::noteNameForFrequency (toneHz) + "\n" + juce::String (toneHz, 1) + " Hz", juce::dontSendNotification);

    // Libellés dépendant du type GEN
    const int gt = (int) proc.apvts.getRawParameterValue ("genType")->load();
    if (gt != lastGenType)
    {
        lastGenType = gt;
        genTone.label.setText (gt == 0 ? "TONE" : "REPLACE", juce::dontSendNotification);
        keyLockBtn.setEnabled (gt == 1);
        keyLockBtn.setAlpha (gt == 1 ? 1.0f : 0.4f);
    }

    const bool playing = proc.hostBpm.load() > 0.0;
    pumpInfo.setText (playing ? "Synced to host\n" + juce::String (proc.hostBpm.load(), 1) + " BPM" : "Free running", juce::dontSendNotification);

    // Modules inactifs grisés
    auto dim = [] (bool on, std::initializer_list<juce::Component*> comps)
    {
        for (auto* c : comps) c->setAlpha (on ? 1.0f : 0.45f);
    };
    auto on = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };
    dim (on ("genOn"),   { genTypeSeg.get(), &genLevel, &genTone });
    dim (on ("toneOn"),  { &toneAmt, &toneQ, &toneHarm, &toneFreqLabel });
    dim (on ("driveOn"), { driveTypeSeg.get(), &drive, &color, &focus, &driveMix });
    dim (on ("shapeOn"), { &attack, &sustain, &shapeInfo });
    dim (on ("pumpOn"),  { pumpRateSeg.get(), &pumpDepth, &pumpShape, &pumpInfo });
    dim (on ("widthOn"), { &width, &widthInfo });
}

void Panel::drawRail (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9c6cf), 0.0f, r.getY(),
                                             juce::Colour (0xff85828c), 0.0f, r.getBottom(), false));
    g.fillRect (r);
    for (float x : { 18.0f, r.getCentreX(), r.getRight() - 18.0f })
    {
        g.setColour (juce::Colour (0xff3a3840));
        g.fillRoundedRectangle (juce::Rectangle<float> (14.0f, 7.0f).withCentre ({ x, r.getCentreY() }), 3.5f);
        g.setColour (juce::Colour (0xffe4e1e8));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x, r.getCentreY() }));
        g.setColour (juce::Colour (0xff6a6770));
        g.drawLine (x - 3.0f, r.getCentreY(), x + 3.0f, r.getCentreY(), 1.3f);
    }
}

void Panel::drawCard (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col, const juce::String& tab)
{
    g.setColour (palette::card);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (col.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.2f);
    g.setColour (col);
    g.fillRoundedRectangle (r.withHeight (4.0f).reduced (14.0f, 0.0f), 2.0f);
    if (tab.isNotEmpty())
    {
        auto t = juce::Rectangle<float> (r.getX() + 14.0f, r.getY() - 9.0f, 16.0f + (float) tab.length() * 8.0f, 18.0f);
        g.fillRoundedRectangle (t, 9.0f);
        g.setColour (palette::ink);
        g.setFont (bold (11.0f));
        g.drawText (tab, t.toNearestInt(), juce::Justification::centred);
    }
}

void Panel::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (palette::panelTop, 0.0f, 0.0f, palette::panelBottom, 0.0f, (float) baseH, false));
    g.fillAll();
    drawRail (g, { 0.0f, 0.0f, (float) baseW, 20.0f });
    drawRail (g, { 0.0f, (float) baseH - 20.0f, (float) baseW, 20.0f });

    // Titre : plaque claire, texte noir
    auto plate = juce::Rectangle<float> (18.0f, 28.0f, 300.0f, 40.0f);
    g.setColour (juce::Colour (0xffece8e1));
    g.fillRoundedRectangle (plate, 6.0f);
    g.setColour (juce::Colour (0xffbdb8b0));
    g.drawRoundedRectangle (plate.reduced (0.5f), 6.0f, 1.0f);
    for (float sx : { plate.getX() + 9.0f, plate.getRight() - 9.0f })
    {
        g.setColour (juce::Colour (0xffb3aea6));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ sx, plate.getCentreY() }));
    }
    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (juce::FontOptions (27.0f, juce::Font::bold)).withExtraKerningFactor (0.06f));
    g.drawText ("SUBSHAPER", juce::Rectangle<float> (plate.getX() + 20.0f, plate.getY(), 190.0f, plate.getHeight()),
                juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff4a4750));
    g.setFont (bold (9.5f));
    g.drawText ("BASS PROCESSOR", juce::Rectangle<float> (plate.getX() + 196.0f, plate.getY() + 7.0f, 96.0f, 13.0f),
                juce::Justification::centredLeft);
    g.setFont (plain (9.5f));
    g.drawText ("v" + juce::String (JucePlugin_VersionString),
                juce::Rectangle<float> (plate.getX() + 196.0f, plate.getY() + 20.0f, 96.0f, 13.0f), juce::Justification::centredLeft);

    // Cartes
    drawCard (g, { 884.0f, 84.0f, 276.0f, 222.0f }, palette::butter, "KEY");
    for (auto& m : modules)
        drawCard (g, m.bounds.toFloat(), m.colour, {});
    drawCard (g, { 20.0f, 640.0f, 1140.0f, 104.0f }, palette::aqua, "MAIN");

}

void Panel::resized()
{
    // Barre du haut
    int x = 470;
    auto place = [&x] (juce::Component& c, int w, int gap = 4) { c.setBounds (x, 34, w, 28); x += w + gap; };
    place (prevBtn, 28);
    place (presetBox, 224);
    place (nextBtn, 28);
    place (saveBtn, 56, 16);
    place (slotA, 30);
    place (slotB, 30);
    place (copyBtn, 56, 16);
    place (undoBtn, 56);
    place (redoBtn, 56, 16);
    place (scaleBox, 72);

    // Écran + KEY
    spectrum.setBounds (20, 84, 850, 222);
    tuner.setBounds (894, 96, 256, 58);
    keyKnob.setBounds (890, 160, 150, 146);
    octaveSeg->setBounds (1050, 166, 98, 104);
    keyReadout.setBounds (1040, 276, 118, 22);

    // Modules
    const int cardW = 182, gap = 9, top = 324, cardH = 300;
    for (size_t i = 0; i < modules.size(); ++i)
        modules[i].bounds = { 20 + (int) i * (cardW + gap), top, cardW, cardH };

    auto cell = [&] (int module, int col, int row) -> juce::Rectangle<int>
    {
        const auto& b = modules[(size_t) module].bounds;
        const int cw = (b.getWidth() - 16) / 2;
        return { b.getX() + 8 + col * cw, b.getY() + 88 + row * 104, cw, 100 };
    };
    auto header = [&] (int module) { const auto& b = modules[(size_t) module].bounds; return juce::Rectangle<int> (b.getX() + 10, b.getY() + 14, b.getWidth() - 20, 34); };
    auto selector = [&] (int module) { const auto& b = modules[(size_t) module].bounds; return juce::Rectangle<int> (b.getX() + 10, b.getY() + 56, b.getWidth() - 20, 24); };
    auto fullRow = [&] (int module, int row) { auto c = cell (module, 0, row); return c.withWidth (modules[(size_t) module].bounds.getWidth() - 16); };

    // GEN
    genOn.setBounds (header (0));
    genTypeSeg->setBounds (selector (0));
    genLevel.setBounds (cell (0, 0, 0));
    genTone.setBounds (cell (0, 1, 0));
    keyLockBtn.setBounds (fullRow (0, 1).withSizeKeepingCentre (fullRow (0, 1).getWidth() - 8, 32));

    // TONE
    toneOn.setBounds (header (1));
    toneAmt.setBounds (cell (1, 0, 0).translated (0, -26));
    toneQ.setBounds (cell (1, 1, 0).translated (0, -26));
    toneHarm.setBounds (cell (1, 0, 1).withTrimmedTop (-16).translated (0, 8));
    toneFreqLabel.setBounds (cell (1, 1, 1).translated (0, 4));

    // DRIVE
    driveOn.setBounds (header (2));
    driveTypeSeg->setBounds (selector (2));
    drive.setBounds (cell (2, 0, 0));
    color.setBounds (cell (2, 1, 0));
    focus.setBounds (cell (2, 0, 1));
    driveMix.setBounds (cell (2, 1, 1));

    // SHAPE
    shapeOn.setBounds (header (3));
    attack.setBounds (cell (3, 0, 0).translated (0, -26));
    sustain.setBounds (cell (3, 1, 0).translated (0, -26));
    shapeInfo.setBounds (fullRow (3, 1).translated (0, -10));

    // PUMP
    pumpOn.setBounds (header (4));
    pumpRateSeg->setBounds (selector (4));
    pumpDepth.setBounds (cell (4, 0, 0));
    pumpShape.setBounds (cell (4, 1, 0));
    pumpInfo.setBounds (fullRow (4, 1));

    // WIDTH
    widthOn.setBounds (header (5));
    width.setBounds (fullRow (5, 0).translated (0, -26).withSizeKeepingCentre (110, 100));
    widthInfo.setBounds (fullRow (5, 1).translated (0, -10));

    // Global
    const int kw = 92;
    int gx = 34;
    for (auto* k : { &inGain, &crossover, &mix, &output })
    {
        k->setBounds (gx, 650, kw, 90);
        gx += kw + 6;
    }
    meter.setBounds (428, 652, 30, 84);
    const int tw = 150, th = 34;
    int col = 0;
    for (auto* t : { &monoLow, &soloLow, &subCut, &delta, &gainMatch, &hq })
    {
        const int r = col / 3, c = col % 3;
        t->setBounds (482 + c * (tw + 10), 654 + r * (th + 10), tw, th);
        ++col;
    }
}

// ============================================================================
//  Éditeur (zoom)
// ============================================================================
SubShaperEditor::SubShaperEditor (SubShaperProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p)
{
    addAndMakeVisible (panel);
    panel.onScaleChange = [this] (float s) { applyScale (s); };
    applyScale ((float) (double) proc.apvts.state.getProperty ("uiScale", 1.0));
}

SubShaperEditor::~SubShaperEditor() = default;

void SubShaperEditor::applyScale (float s)
{
    scale = juce::jlimit (1.0f, 1.5f, s);
    panel.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (Panel::baseW * scale), juce::roundToInt (Panel::baseH * scale));
}

void SubShaperEditor::resized()
{
    panel.setBounds (0, 0, Panel::baseW, Panel::baseH);
}

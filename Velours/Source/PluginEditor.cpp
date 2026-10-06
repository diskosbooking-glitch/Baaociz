#include "PluginEditor.h"

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
//  Look & feel (identique à Subshaper pour la cohérence de la gamme)
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
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (palette::textDim.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        g.drawLine ({ c.getPointOnCircumference (r * 0.93f, a), c.getPointOnCircumference (r, a) }, i % 5 == 0 ? 1.8f : 1.0f);
    }

    const float arcR = r * 0.83f, arcW = juce::jmax (2.5f, r * 0.07f);
    const juce::PathStrokeType stroke (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff34333b));
    g.strokePath (bgArc, stroke);

    const bool bipolar = s.getMinimum() < 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path valArc;
    valArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (col);
    g.strokePath (valArc, stroke);

    const float skirtR = r * 0.68f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - skirtR, c.y - skirtR + r * 0.05f, skirtR * 2.0f, skirtR * 2.0f);
    g.setColour (col.darker (0.55f).withSaturation (col.getSaturation() * 0.7f));
    g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);
    g.setColour (col.darker (0.95f).withAlpha (0.8f));
    for (int i = 0; i < 24; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f + angle;
        g.drawLine ({ c.getPointOnCircumference (skirtR * 0.8f, a), c.getPointOnCircumference (skirtR * 0.98f, a) }, 1.4f);
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
        g.setFont (bold (juce::jmin (12.5f, r.getHeight() * 0.5f, r.getWidth() * 0.42f)));
        g.drawFittedText (b.getButtonText(), r.toNearestInt().reduced (2, 0), juce::Justification::centred, 1);
        return;
    }

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
    drawLedPanel (g, b, getAccent (b), b.getToggleState(), over, 12.5f);
}

// ============================================================================
//  Écran central
// ============================================================================
static constexpr float fMin = 20.0f, fMax = 20000.0f;
static constexpr float specTop = 0.0f, specBottom = -96.0f;   // dBFS (pente +4,5 dB/oct autour de 1 kHz)
static constexpr float sensRange = 18.0f;                     // ± dB affichés pour les bandes
static constexpr float redRange = 24.0f;                      // dB de réduction en haut de l'écran

SpectrumDisplay::SpectrumDisplay (VeloursProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void SpectrumDisplay::resized()
{
    plot = getLocalBounds().toFloat().reduced (4.0f, 3.0f).withTrimmedBottom (16.0f);
    const int w = juce::jmax (1, (int) plot.getWidth());
    colIn.assign ((size_t) w, -200.0f);
    colOut.assign ((size_t) w, -200.0f);
    colRed.assign ((size_t) w, 0.0f);
    colLo.assign ((size_t) w, 0);
    colHi.assign ((size_t) w, 0);
    mappedBins = -1;
    for (auto& c : sensCurve) c.assign ((size_t) w, 0.0f);
    curveChannels = 0;
    rebuildCurves();
}

float SpectrumDisplay::xForFreq (float f) const
{
    return plot.getX() + plot.getWidth() * std::log (juce::jlimit (fMin, fMax, f) / fMin) / std::log (fMax / fMin);
}

float SpectrumDisplay::freqForX (float x) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return fMin * std::pow (fMax / fMin, t);
}

float SpectrumDisplay::yForSens (float db) const
{
    return plot.getY() + plot.getHeight() * (0.5f - 0.42f * juce::jlimit (-sensRange, sensRange, db) / sensRange);
}

float SpectrumDisplay::sensForY (float y) const
{
    return juce::jlimit (-12.0f, 12.0f, (0.5f - (y - plot.getY()) / plot.getHeight()) / 0.42f * sensRange);
}

float SpectrumDisplay::yForSpec (float db, float f) const
{
    const float tilted = db + 4.5f * std::log2 (juce::jmax (1.0f, f) / 1000.0f);
    const float t = (tilted - specTop) / (specBottom - specTop);
    return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.05f, t);
}

float SpectrumDisplay::yForRed (float db) const
{
    return plot.getY() + plot.getHeight() * 0.5f * juce::jlimit (0.0f, 1.0f, db / redRange);
}

juce::RangedAudioParameter* SpectrumDisplay::bandParam (int band, const char* what) const
{
    return proc.apvts.getParameter (params::bandId (band, what));
}

void SpectrumDisplay::setBandValue (int band, const char* what, float value, bool gesture)
{
    if (auto* p = bandParam (band, what))
    {
        if (gesture) p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        if (gesture) p->endChangeGesture();
    }
}

juce::Point<float> SpectrumDisplay::nodePos (int band) const
{
    const auto& b = bandsNow[(size_t) band];
    const float y = usesGain (b.type) ? yForSens (b.gain) : yForSens (0.0f);
    return { xForFreq (b.freq), y };
}

int SpectrumDisplay::hitBand (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 12.0f;
    for (int b = 0; b < params::numBands; ++b)
    {
        if (! bandsNow[(size_t) b].on) continue;
        const float d = nodePos (b).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void SpectrumDisplay::rebuildCurves()
{
    const int w = (int) colIn.size();
    const int nc = proc.processChannels.load();
    const bool ms = proc.apvts.getRawParameterValue ("stereo")->load() > 0.5f;
    curveChannels = nc;
    curveMS = ms;
    curvesDiffer = false;
    for (int c = 0; c < 2; ++c)
        for (int x = 0; x < w; ++x)
        {
            const float f = freqForX (plot.getX() + (float) x);
            sensCurve[c][(size_t) x] = bands::totalDb (bandsNow.data(), params::numBands, f, c, nc, ms);
        }
    if (nc > 1)
        for (int x = 0; x < w; ++x)
            if (std::abs (sensCurve[0][(size_t) x] - sensCurve[1][(size_t) x]) > 0.05f) { curvesDiffer = true; break; }
}

void SpectrumDisplay::timerCallback()
{
    // Bandes (automation, presets, glisser)
    const auto b = proc.readBands();
    const bool ms = proc.apvts.getRawParameterValue ("stereo")->load() > 0.5f;
    if (b != bandsNow || curveChannels != proc.processChannels.load() || ms != curveMS)
    {
        bandsNow = b;
        rebuildCurves();
    }

    // Spectres
    auto& ui = proc.engine.ui;
    const int w = (int) colIn.size();
    bool fresh = false;
    {
        const juce::SpinLock::ScopedLockType sl (ui.lock);
        if (ui.bins > 0 && ui.counter != lastCounter)
        {
            fresh = true;
            lastCounter = ui.counter;
            if (ui.bins != mappedBins || ! juce::exactlyEqual (ui.binHz, mappedBinHz))
            {
                mappedBins = ui.bins;
                mappedBinHz = ui.binHz;
                for (int x = 0; x < w; ++x)
                {
                    const float f0 = freqForX (plot.getX() + (float) x - 0.5f);
                    const float f1 = freqForX (plot.getX() + (float) x + 0.5f);
                    int lo = (int) std::floor (f0 / ui.binHz), hi = (int) std::ceil (f1 / ui.binHz);
                    lo = juce::jlimit (1, ui.bins - 1, lo);
                    hi = juce::jlimit (lo, ui.bins - 1, hi);
                    colLo[(size_t) x] = lo;
                    colHi[(size_t) x] = hi;
                }
            }
            scratchIn.resize ((size_t) w);
            scratchOut.resize ((size_t) w);
            scratchRed.resize ((size_t) w);
            for (int x = 0; x < w; ++x)
            {
                float a = -200.0f, o = -200.0f, r = 0.0f;
                if (colHi[(size_t) x] - colLo[(size_t) x] <= 1)
                {
                    // Colonne plus étroite qu'une case : interpolation (pas d'escaliers dans le grave)
                    const float fb = juce::jlimit (1.0f, (float) (ui.bins - 2), freqForX (plot.getX() + (float) x) / ui.binHz);
                    const int k0 = (int) fb;
                    const float t = fb - (float) k0;
                    auto lerp = [k0, t] (const std::vector<float>& v) { return v[(size_t) k0] + t * (v[(size_t) k0 + 1] - v[(size_t) k0]); };
                    a = lerp (ui.inDb);
                    o = lerp (ui.outDb);
                    r = lerp (ui.redDb);
                }
                else
                {
                    for (int k = colLo[(size_t) x]; k <= colHi[(size_t) x]; ++k)
                    {
                        a = juce::jmax (a, ui.inDb[(size_t) k]);
                        o = juce::jmax (o, ui.outDb[(size_t) k]);
                        r = juce::jmax (r, ui.redDb[(size_t) k]);
                    }
                }
                scratchIn[(size_t) x] = a;
                scratchOut[(size_t) x] = o;
                scratchRed[(size_t) x] = r;
            }
        }
    }

    for (int x = 0; x < w; ++x)
    {
        const float tIn = fresh ? scratchIn[(size_t) x] : -200.0f;
        const float tOut = fresh ? scratchOut[(size_t) x] : -200.0f;
        const float tRed = fresh ? scratchRed[(size_t) x] : 0.0f;
        auto& a = colIn[(size_t) x];
        auto& o = colOut[(size_t) x];
        auto& r = colRed[(size_t) x];
        a = tIn > a ? a + 0.7f * (tIn - a) : juce::jmax (tIn, a - 1.6f);
        o = tOut > o ? o + 0.7f * (tOut - o) : juce::jmax (tOut, o - 1.6f);
        r = tRed > r ? r + 0.8f * (tRed - r) : r + 0.25f * (tRed - r);
    }
    repaint();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour (palette::screen);
    g.fillRoundedRectangle (area, 8.0f);
    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    // Grille
    g.setFont (plain (10.5f));
    for (float f : { 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 200.0f, 300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
                     3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f })
    {
        g.setColour (palette::grid);
        g.drawVerticalLine ((int) xForFreq (f), plot.getY(), plot.getBottom());
    }
    for (float f : { 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f })
    {
        g.setColour (palette::edge.withAlpha (0.55f));
        g.drawVerticalLine ((int) xForFreq (f), plot.getY(), plot.getBottom());
    }
    g.setColour (palette::grid);
    for (float db : { -12.0f, -6.0f, 6.0f, 12.0f })
        g.drawHorizontalLine ((int) yForSens (db), plot.getX(), plot.getRight());
    g.setColour (palette::edge.withAlpha (0.6f));
    g.drawHorizontalLine ((int) yForSens (0.0f), plot.getX(), plot.getRight());

    const int w = (int) colIn.size();
    auto colX = [this] (int x) { return plot.getX() + (float) x; };

    // Zones non traitées (sensibilité quasi nulle) : grisées
    {
        const int nc = juce::jmax (1, curveChannels);
        for (int x = 0; x < w; ++x)
        {
            float wMax = 0.0f;
            for (int c = 0; c < juce::jmin (2, nc); ++c)
                wMax = juce::jmax (wMax, bands::weightFromDb (sensCurve[c][(size_t) x]));
            if (wMax < 0.5f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.38f * (1.0f - wMax * 2.0f)));
                g.fillRect (colX (x), plot.getY(), 1.0f, plot.getHeight());
            }
        }
    }

    // Spectre d'entrée (rempli) et de sortie (trait)
    if (w > 1)
    {
        juce::Path inPath, outPath;
        inPath.startNewSubPath (colX (0), plot.getBottom());
        for (int x = 0; x < w; ++x)
        {
            const float f = freqForX (colX (x));
            inPath.lineTo (colX (x), yForSpec (colIn[(size_t) x], f));
            const float yo = yForSpec (colOut[(size_t) x], f);
            if (x == 0) outPath.startNewSubPath (colX (x), yo); else outPath.lineTo (colX (x), yo);
        }
        inPath.lineTo (colX (w - 1), plot.getBottom());
        inPath.closeSubPath();
        g.setGradientFill (juce::ColourGradient (palette::textDim.withAlpha (0.30f), 0.0f, plot.getY(),
                                                 palette::textDim.withAlpha (0.06f), 0.0f, plot.getBottom(), false));
        g.fillPath (inPath);
        g.setColour (palette::textDim.withAlpha (0.45f));
        g.strokePath (inPath, juce::PathStrokeType (1.0f));
        g.setColour (palette::sky.withAlpha (0.85f));
        g.strokePath (outPath, juce::PathStrokeType (1.4f));

        // Réduction : courbe qui descend du haut de l'écran
        juce::Path red;
        red.startNewSubPath (colX (0), plot.getY());
        for (int x = 0; x < w; ++x)
            red.lineTo (colX (x), yForRed (colRed[(size_t) x]));
        red.lineTo (colX (w - 1), plot.getY());
        red.closeSubPath();
        g.setGradientFill (juce::ColourGradient (palette::coral.withAlpha (0.10f), 0.0f, plot.getY(),
                                                 palette::coral.withAlpha (0.55f), 0.0f, yForRed (redRange), false));
        g.fillPath (red);
        juce::Path redLine;
        for (int x = 0; x < w; ++x)
        {
            const float y = yForRed (colRed[(size_t) x]);
            if (x == 0) redLine.startNewSubPath (colX (x), y); else redLine.lineTo (colX (x), y);
        }
        g.setColour (palette::coral);
        g.strokePath (redLine, juce::PathStrokeType (1.8f));
    }

    // Courbe de sensibilité (éditeur de bandes)
    for (int c = (curvesDiffer ? 1 : 0); c >= 0; --c)
    {
        juce::Path s;
        for (int x = 0; x < w; ++x)
        {
            const float y = yForSens (juce::jmax (-sensRange, sensCurve[c][(size_t) x]));
            if (x == 0) s.startNewSubPath (colX (x), y); else s.lineTo (colX (x), y);
        }
        const auto col = curvesDiffer ? (c == 0 ? palette::butter : palette::lavender) : palette::butter;
        g.setColour (col.withAlpha (0.9f));
        if (curvesDiffer && c == 1)
        {
            juce::Path dashed;
            const float dashes[] = { 6.0f, 4.0f };
            juce::PathStrokeType (1.6f).createDashedStroke (dashed, s, dashes, 2);
            g.fillPath (dashed);
        }
        else
            g.strokePath (s, juce::PathStrokeType (1.8f));
    }

    // Nœuds des bandes
    for (int b = 0; b < params::numBands; ++b)
    {
        if (! bandsNow[(size_t) b].on) continue;
        const auto p = nodePos (b);
        const auto col = palette::band (b);
        const bool sel = b == selected, hov = b == hover;
        const float r = sel ? 9.0f : 8.0f;
        if (sel || hov)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (r * 3.2f, r * 3.2f).withCentre (p));
        }
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
        g.setColour (palette::ink);
        g.setFont (bold (11.0f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), juce::Justification::centred);
        if (sel)
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (p), 1.4f);
        }
        const int focus = bandsNow[(size_t) b].focus;
        if (focus != params::focusAll)
        {
            g.setColour (col);
            g.setFont (bold (9.5f));
            g.drawText (juce::String ("ALRMS").substring (focus, focus + 1), juce::Rectangle<float> (14.0f, 12.0f).withCentre (p.translated (0.0f, -r - 9.0f)),
                        juce::Justification::centred);
        }
    }

    // Infos de la bande sélectionnée / survolée
    const int infoBand = dragging >= 0 ? dragging : (hover >= 0 ? hover : selected);
    if (infoBand >= 0 && bandsNow[(size_t) infoBand].on)
    {
        const auto& b = bandsNow[(size_t) infoBand];
        juce::String t = "BAND " + juce::String (infoBand + 1) + "   " + params::bandTypeNames[b.type].toUpperCase()
                       + "   " + params::freqText (b.freq);
        if (usesGain (b.type)) t << "   " << (b.gain > 0.05f ? "+" : "") << juce::String (b.gain, 1) << " dB";
        t << "   Q " << juce::String (b.q, 2) << "   " << params::bandFocusNames[b.focus].toUpperCase();
        const auto font = bold (12.0f);
        auto box = juce::Rectangle<float> (plot.getX() + 10.0f, plot.getY() + 10.0f,
                                           20.0f + juce::GlyphArrangement::getStringWidth (font, t), 24.0f);
        g.setColour (palette::card.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (palette::band (infoBand));
        g.drawRoundedRectangle (box, 6.0f, 1.2f);
        g.setFont (font);
        g.drawText (t, box.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (palette::textDim.withAlpha (0.8f));
        g.setFont (plain (11.5f));
        g.drawText ("Double-click: add band   |   Drag: move   |   Wheel: Q   |   Right-click: type, focus, delete",
                    juce::Rectangle<float> (plot.getX() + 12.0f, plot.getY() + 8.0f, 600.0f, 18.0f), juce::Justification::centredLeft);
    }

    // Fréquence sous la souris
    if (showHover && dragging < 0)
    {
        g.setColour (palette::textDim.withAlpha (0.35f));
        g.drawVerticalLine ((int) hoverPos.x, plot.getY(), plot.getBottom());
        g.setColour (palette::text.withAlpha (0.8f));
        g.setFont (bold (11.0f));
        g.drawText (params::freqText (freqForX (hoverPos.x)), juce::Rectangle<float> (70.0f, 16.0f).withCentre ({ hoverPos.x, plot.getBottom() - 34.0f }),
                    juce::Justification::centred);
    }

    // Échelle de réduction
    g.setColour (palette::coral.withAlpha (0.75f));
    g.setFont (plain (10.0f));
    for (float db : { 6.0f, 12.0f, 18.0f })
        g.drawText ("-" + juce::String ((int) db), juce::Rectangle<float> (plot.getRight() - 34.0f, yForRed (db) - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredRight);

    // Légende
    {
        float lx = plot.getX() + 12.0f;
        const float ly = plot.getBottom() - 18.0f;
        g.setFont (bold (10.5f));
        const std::pair<juce::Colour, const char*> items[] = { { palette::textDim, "INPUT" }, { palette::sky, "OUTPUT" },
                                                               { palette::coral, "REDUCTION" }, { palette::butter, "SENSITIVITY" } };
        for (const auto& it : items)
        {
            g.setColour (it.first);
            g.fillRoundedRectangle (lx, ly + 5.0f, 12.0f, 4.0f, 2.0f);
            g.setColour (it.first.withAlpha (0.9f));
            const float tw = (float) juce::String (it.second).length() * 6.6f + 8.0f;
            g.drawText (it.second, juce::Rectangle<float> (lx + 16.0f, ly, tw, 14.0f), juce::Justification::centredLeft);
            lx += 16.0f + tw + 10.0f;
        }
    }

    g.restoreState();

    // Étiquettes de fréquence
    g.setColour (palette::textDim);
    g.setFont (plain (10.5f));
    const std::pair<float, const char*> labels[] = { { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
                                                     { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" } };
    for (const auto& l : labels)
        g.drawText (l.second, juce::Rectangle<float> (40.0f, 16.0f).withCentre ({ xForFreq (l.first), plot.getBottom() + 9.0f }),
                    juce::Justification::centred);

    g.setColour (palette::edge);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);
}

// ------------------------------------------------------------------ souris
void SpectrumDisplay::mouseMove (const juce::MouseEvent& e)
{
    hover = hitBand (e.position);
    hoverPos = e.position;
    showHover = plot.contains (e.position);
    setMouseCursor (hover >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    showHover = false;
}

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{
    const int b = hitBand (e.position);
    if (e.mods.isPopupMenu())
    {
        if (b >= 0) { selected = b; showBandMenu (b); }
        else showAddMenu (e.position);
        return;
    }
    selected = b;
    dragging = b;
    if (b >= 0)
        for (auto* what : { "freq", "gain" })
            if (auto* p = bandParam (b, what)) p->beginChangeGesture();
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const auto pos = e.position;
    const float f = freqForX (juce::jlimit (plot.getX(), plot.getRight(), pos.x));
    setBandValue (dragging, "freq", f, false);
    if (usesGain (bandsNow[(size_t) dragging].type) && ! e.mods.isShiftDown())
        setBandValue (dragging, "gain", sensForY (pos.y), false);
    bandsNow = proc.readBands();
    rebuildCurves();
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        for (auto* what : { "freq", "gain" })
            if (auto* p = bandParam (dragging, what)) p->endChangeGesture();
    dragging = -1;
}

void SpectrumDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = hitBand (e.position);
    if (b >= 0)
    {
        setBandValue (b, "on", 0.0f);
        if (selected == b) selected = -1;
        hover = -1;
        return;
    }
    if (plot.contains (e.position))
        addBand (freqForX (e.position.x), sensForY (e.position.y), params::bell);
}

void SpectrumDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int b = hitBand (e.position);
    if (b < 0) b = selected;
    if (b < 0 || ! bandsNow[(size_t) b].on) return;
    const float q = bandsNow[(size_t) b].q * std::pow (2.0f, wheel.deltaY * (wheel.isReversed ? -1.5f : 1.5f));
    setBandValue (b, "q", juce::jlimit (0.2f, 10.0f, q));
}

void SpectrumDisplay::addBand (float freq, float gain, int type)
{
    for (int b = 0; b < params::numBands; ++b)
        if (! bandsNow[(size_t) b].on)
        {
            setBandValue (b, "type", (float) type);
            setBandValue (b, "freq", freq);
            setBandValue (b, "gain", type == params::lowCut || type == params::highCut ? 0.0f : gain);
            setBandValue (b, "q", type == params::lowCut || type == params::highCut ? 0.71f : 1.0f);
            setBandValue (b, "focus", (float) params::focusAll);
            setBandValue (b, "on", 1.0f);
            selected = b;
            return;
        }
}

void SpectrumDisplay::showBandMenu (int band)
{
    juce::PopupMenu m;
    const auto& b = bandsNow[(size_t) band];
    m.addSectionHeader ("BAND " + juce::String (band + 1));
    for (int t = 0; t < params::numBandTypes; ++t)
        m.addItem (100 + t, params::bandTypeNames[t], true, b.type == t);
    juce::PopupMenu focus;
    for (int f = 0; f < params::bandFocusNames.size(); ++f)
        focus.addItem (200 + f, params::bandFocusNames[f], true, b.focus == f);
    m.addSeparator();
    m.addSubMenu ("Focus (stereo)", focus);
    m.addItem (300, "Reset sensitivity to 0 dB");
    m.addSeparator();
    m.addItem (400, "Delete band");

    juce::Component::SafePointer<SpectrumDisplay> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, band] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= 100 && r < 200) safe->setBandValue (band, "type", (float) (r - 100));
        else if (r >= 200 && r < 300) safe->setBandValue (band, "focus", (float) (r - 200));
        else if (r == 300) safe->setBandValue (band, "gain", 0.0f);
        else if (r == 400) { safe->setBandValue (band, "on", 0.0f); safe->selected = -1; }
    });
}

void SpectrumDisplay::showAddMenu (juce::Point<float> pos)
{
    if (! plot.contains (pos)) return;
    juce::PopupMenu m;
    const float f = freqForX (pos.x), gain = sensForY (pos.y);
    m.addSectionHeader ("ADD BAND AT " + params::freqText (f));
    bool anyFree = false;
    for (const auto& b : bandsNow) anyFree |= ! b.on;
    for (int t = 0; t < params::numBandTypes; ++t)
        m.addItem (100 + t, params::bandTypeNames[t], anyFree);
    if (! anyFree) m.addItem (999, "(8 bands max)", false);

    juce::Component::SafePointer<SpectrumDisplay> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, f, gain] (int r)
    {
        if (safe != nullptr && r >= 100 && r < 200) safe->addBand (f, gain, r - 100);
    });
}

// ============================================================================
//  Potards, sélecteurs, vumètre
// ============================================================================
LabelledKnob::LabelledKnob()
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, palette::text);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (bold (11.5f));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void LabelledKnob::attach (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text,
                           juce::Colour c, const juce::String& tip, bool big)
{
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
    label.setText (text, juce::dontSendNotification);
    label.setFont (bold (big ? 13.0f : 11.5f));
    setAccent (slider, c);
    label.setColour (juce::Label::textColourId, c);
    slider.setTooltip (tip);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, big ? 20 : 18);
}

void LabelledKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

ChoiceSegments::ChoiceSegments (juce::RangedAudioParameter& param, const juce::StringArray& labels, juce::Colour accent)
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
    });
    attachment->sendInitialUpdate();
}

void ChoiceSegments::resized()
{
    auto r = getLocalBounds();
    const int n = buttons.size(), gap = 4;
    for (int i = 0; i < n; ++i)
    {
        const int w = (r.getWidth() - gap * (n - 1 - i)) / (n - i);
        buttons[i]->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (gap);
    }
}

void ReductionMeter::timerCallback()
{
    const float o = proc.engine.overallReductionDb.load();
    const float p = proc.engine.peakReductionDb.load();
    overall += (o - overall) * 0.25f;
    peak += (p - peak) * (p > peak ? 0.6f : 0.15f);
    if (p > peakHold) { peakHold = p; holdFrames = 45; }
    else if (--holdFrames < 0) peakHold = juce::jmax (p, peakHold - 0.3f);
    repaint();
}

void ReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (palette::screen);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (palette::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

    auto inner = r.reduced (10.0f, 8.0f);
    g.setColour (juce::Colour (0xffb5b2bf));
    g.setFont (bold (10.5f));
    g.drawText ("LEVEL CHANGE", inner.removeFromTop (14.0f), juce::Justification::centredLeft);
    g.setColour (palette::coral);
    g.setFont (bold (21.0f));
    g.drawText ((overall > 0.05f ? "-" : "") + juce::String (overall, 1) + " dB", inner.removeFromTop (26.0f), juce::Justification::centredLeft);

    inner.removeFromTop (4.0f);
    g.setColour (juce::Colour (0xffb5b2bf));
    g.setFont (bold (10.5f));
    g.drawText ("DEEPEST CUT", inner.removeFromTop (14.0f), juce::Justification::centredLeft);
    auto bar = inner.removeFromTop (10.0f);
    g.setColour (juce::Colour (0xff2a2930));
    g.fillRoundedRectangle (bar, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, peak / 24.0f);
    g.setColour (palette::coral);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 3.0f);
    const float hx = bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, peakHold / 24.0f);
    g.setColour (palette::text);
    g.fillRect (hx - 1.0f, bar.getY(), 2.0f, bar.getHeight());
    g.setColour (palette::textDim);
    g.setFont (plain (10.0f));
    g.drawText ("-" + juce::String (peakHold, 1) + " dB", inner.removeFromTop (14.0f), juce::Justification::centredRight);
}

// ============================================================================
//  Panneau principal
// ============================================================================
Panel::Panel (VeloursProcessor& p) : proc (p), display (p), meter (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    addAndMakeVisible (display);

    depth.attach (s, "depth", "DEPTH", palette::peach, "How much the resonances are reduced", true);
    detail.attach (s, "detail", "DETAIL", palette::lavender,
                   "Low = broad, smooth processing. High = narrow, surgical cuts on the most prominent resonances", true);
    detailTilt.attach (s, "detailTilt", "DETAIL TILT", palette::lavender, "Changes Detail with frequency: + = more detail in the highs, - = in the lows");
    attack.attach (s, "attack", "ATTACK", palette::mint, "How fast the cuts react");
    release.attach (s, "release", "RELEASE", palette::mint, "How fast the cuts recover");
    timeTilt.attach (s, "timeTilt", "TIME TILT", palette::mint, "Changes attack/release with frequency: + = faster highs and slower lows");
    maxCut.attach (s, "maxcut", "MAX CUT", palette::coral, "Hard limit on the attenuation applied at any frequency");
    link.attach (s, "link", "LINK", palette::sky, "Stereo link: 100 % = same processing on both channels (image preserved)");
    mix.attach (s, "mix", "MIX", palette::butter, "Dry / processed balance (phase-aligned)");
    wetTrim.attach (s, "wetTrim", "WET TRIM", palette::butter, "Gain of the processed signal only");
    output.attach (s, "output", "OUTPUT", palette::butter, "Output gain");

    for (auto* k : { &depth, &detail, &detailTilt, &attack, &release, &timeTilt, &maxCut, &link, &mix, &wetTrim, &output })
        addAndMakeVisible (k);

    modeSeg = std::make_unique<ChoiceSegments> (*s.getParameter ("mode"), juce::StringArray { "SOFT", "HARD" }, palette::peach);
    modeSeg->setTip ("Soft: transparent, follows the spectral shape only. Hard: also reacts to sudden peaks, more compressor-like");
    stereoSeg = std::make_unique<ChoiceSegments> (*s.getParameter ("stereo"), juce::StringArray { "L / R", "M / S" }, palette::sky);
    stereoSeg->setTip ("Process left/right or mid/side. Bands can target one channel with Focus (right-click a band)");
    addAndMakeVisible (*modeSeg);
    addAndMakeVisible (*stereoSeg);

    attachToggle (sidechain, "sidechain", "SIDECHAIN", palette::sky, "Detect resonances on the external sidechain input");
    attachToggle (delta, "delta", "DELTA", palette::rose, "Listen only to what is being removed");
    attachToggle (bypass, "bypass", "BYPASS", palette::coral, "Bypass (latency compensated)");

    addAndMakeVisible (meter);
    scInfo.setFont (plain (10.5f));
    scInfo.setColour (juce::Label::textColourId, palette::textDim);
    scInfo.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (scInfo);

    // Presets
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    addAndMakeVisible (presetBox);
    for (auto* b : { &prevBtn, &nextBtn }) setAccent (*b, palette::butter);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    const auto& list = presets::factory();
    for (int i = 0; i < (int) list.size(); ++i)
        presetBox.addItem (list[(size_t) i].name, i + 1);
    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) proc.loadFactoryPreset (id - 1);
        shownPreset = proc.getPresetName();
    };
    presetBox.setTooltip ("Factory presets");

    // Résolution
    qualityBox.addItemList (params::qualityNames, 1);
    qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, "quality", qualityBox);
    qualityBox.setTooltip ("Low Latency: 21 ms. Normal: 43 ms, finer. High Res: 85 ms, finest frequency resolution");
    addAndMakeVisible (qualityBox);

    // Zoom
    const juce::StringArray scales { "75%", "100%", "125%", "150%" };
    scaleBox.addItemList (scales, 1);
    const float current = (float) (double) proc.apvts.state.getProperty ("uiScale", 1.0);
    scaleBox.setSelectedId (current < 0.8f ? 1 : current < 1.1f ? 2 : current < 1.3f ? 3 : 4, juce::dontSendNotification);
    scaleBox.onChange = [this]
    {
        const float v[] = { 0.75f, 1.0f, 1.25f, 1.5f };
        const float sc = v[juce::jlimit (0, 3, scaleBox.getSelectedId() - 1)];
        proc.apvts.state.setProperty ("uiScale", sc, nullptr);
        if (onScaleChange) onScaleChange (sc);
    };
    addAndMakeVisible (scaleBox);

    refreshPresetBox();
    startTimerHz (10);
}

Panel::~Panel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Panel::attachToggle (juce::ToggleButton& t, const juce::String& id, const juce::String& text, juce::Colour c, const juce::String& tip)
{
    t.setButtonText (text);
    setAccent (t, c);
    t.setTooltip (tip);
    t.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t));
    addAndMakeVisible (t);
}

void Panel::refreshPresetBox()
{
    shownPreset = proc.getPresetName();
    const auto& list = presets::factory();
    int found = 0;
    for (int i = 0; i < (int) list.size(); ++i)
        if (shownPreset == list[(size_t) i].name) found = i + 1;
    if (found > 0) presetBox.setSelectedId (found, juce::dontSendNotification);
    else presetBox.setText (shownPreset, juce::dontSendNotification);
}

void Panel::stepPreset (int step)
{
    const int n = (int) presets::factory().size();
    int cur = presetBox.getSelectedId() - 1;
    if (cur < 0) cur = 0;
    const int next = ((cur + step) % n + n) % n;
    presetBox.setSelectedId (next + 1, juce::sendNotificationSync);
}

void Panel::timerCallback()
{
    if (proc.getPresetName() != shownPreset)
        refreshPresetBox();

    const bool scOn = proc.apvts.getRawParameterValue ("sidechain")->load() > 0.5f;
    const bool connected = proc.sidechainConnected.load();
    const juce::String t = ! scOn ? juce::String ("Detects on the main input")
                         : connected ? juce::String ("Detecting on sidechain")
                                     : juce::String ("No sidechain routed: main input used");
    if (scInfo.getText() != t) scInfo.setText (t, juce::dontSendNotification);
    scInfo.setColour (juce::Label::textColourId, scOn && ! connected ? palette::coral : palette::textDim);

    const bool stereo = proc.processChannels.load() > 1;
    link.setEnabled (stereo);
    link.setAlpha (stereo ? 1.0f : 0.4f);
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
        auto t = juce::Rectangle<float> (r.getX() + 14.0f, r.getY() - 9.0f, 18.0f + (float) tab.length() * 8.0f, 18.0f);
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

    // Plaque titre
    auto plate = juce::Rectangle<float> (20.0f, 28.0f, 330.0f, 40.0f);
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
    g.setFont (juce::Font (juce::FontOptions (27.0f, juce::Font::bold)).withExtraKerningFactor (0.08f));
    g.drawText ("VELOURS", juce::Rectangle<float> (plate.getX() + 20.0f, plate.getY(), 170.0f, plate.getHeight()),
                juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff4a4750));
    g.setFont (bold (9.5f));
    g.drawText ("RESONANCE SUPPRESSOR", juce::Rectangle<float> (plate.getX() + 186.0f, plate.getY() + 7.0f, 136.0f, 13.0f),
                juce::Justification::centredLeft);
    g.setFont (plain (9.5f));
    g.drawText ("Aociz  -  v" + juce::String (JucePlugin_VersionString),
                juce::Rectangle<float> (plate.getX() + 186.0f, plate.getY() + 20.0f, 136.0f, 13.0f), juce::Justification::centredLeft);

    for (const auto& c : cards)
        drawCard (g, c.bounds.toFloat(), c.colour, c.tab);
}

void Panel::resized()
{
    // Barre du haut (alignée à droite)
    int x = baseW - 20 - 678;
    auto place = [&x] (juce::Component& c, int w, int gap = 6) { c.setBounds (x, 34, w, 28); x += w + gap; };
    place (prevBtn, 28, 4);
    place (presetBox, 250, 4);
    place (nextBtn, 28, 18);
    place (qualityBox, 132, 10);
    place (scaleBox, 74, 18);
    place (bypass, 120, 0);

    display.setBounds (20, 82, baseW - 40, 330);

    // Cartes
    cards.clear();
    const int top = 436, h = 244, gap = 12;
    int cx = 20;
    auto addCard = [&] (int w, juce::Colour c, const juce::String& tab) { cards.push_back ({ { cx, top, w, h }, c, tab }); cx += w + gap; return cards.back().bounds; };

    const auto shape = addCard (262, palette::peach, "SHAPE");
    const auto time  = addCard (196, palette::mint, "TIME");
    const auto limit = addCard (150, palette::coral, "LIMIT");
    const auto st    = addCard (190, palette::sky, "STEREO");
    const auto out   = addCard (baseW - 20 - cx, palette::butter, "OUTPUT");

    // SHAPE : DEPTH + DETAIL (gros), SOFT/HARD + DETAIL TILT
    depth.setBounds (shape.getX() + 12, shape.getY() + 20, 116, 128);
    detail.setBounds (shape.getRight() - 128, shape.getY() + 20, 116, 128);
    modeSeg->setBounds (shape.getX() + 18, shape.getY() + 176, 130, 30);
    detailTilt.setBounds (shape.getRight() - 112, shape.getY() + 152, 92, 88);

    // TIME
    attack.setBounds (time.getX() + 10, time.getY() + 24, 86, 104);
    release.setBounds (time.getRight() - 96, time.getY() + 24, 86, 104);
    timeTilt.setBounds (time.getCentreX() - 46, time.getY() + 140, 92, 98);

    // LIMIT
    maxCut.setBounds (limit.getCentreX() - 46, limit.getY() + 24, 92, 104);
    meter.setBounds (limit.getX() + 10, limit.getY() + 138, limit.getWidth() - 20, 96);

    // STEREO
    stereoSeg->setBounds (st.getX() + 14, st.getY() + 22, st.getWidth() - 28, 28);
    link.setBounds (st.getCentreX() - 46, st.getY() + 58, 92, 100);
    sidechain.setBounds (st.getX() + 14, st.getY() + 170, st.getWidth() - 28, 32);
    scInfo.setBounds (st.getX() + 6, st.getY() + 206, st.getWidth() - 12, 30);

    // OUTPUT
    const int kw = (out.getWidth() - 20) / 3;
    mix.setBounds (out.getX() + 10, out.getY() + 24, kw, 104);
    wetTrim.setBounds (out.getX() + 10 + kw, out.getY() + 24, kw, 104);
    output.setBounds (out.getX() + 10 + 2 * kw, out.getY() + 24, kw, 104);
    delta.setBounds (out.getX() + 16, out.getY() + 170, out.getWidth() - 32, 32);
}

// ============================================================================
//  Éditeur (zoom)
// ============================================================================
VeloursEditor::VeloursEditor (VeloursProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p)
{
    addAndMakeVisible (panel);
    panel.onScaleChange = [this] (float s) { applyScale (s); };
    applyScale ((float) (double) proc.apvts.state.getProperty ("uiScale", 1.0));
}

VeloursEditor::~VeloursEditor() = default;

void VeloursEditor::applyScale (float s)
{
    scale = juce::jlimit (0.75f, 1.5f, s);
    panel.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (Panel::baseW * scale), juce::roundToInt (Panel::baseH * scale));
}

void VeloursEditor::resized()
{
    panel.setBounds (0, 0, Panel::baseW, Panel::baseH);
}

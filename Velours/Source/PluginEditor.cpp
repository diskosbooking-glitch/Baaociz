#include "PluginEditor.h"
#include "BinaryData.h"

// ============================================================================
//  Typographie (Inter, licence SIL OFL, embarquée)
// ============================================================================
namespace
{
juce::Typeface::Ptr interRegularTf()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, (size_t) BinaryData::InterRegular_ttfSize);
    return tf;
}
juce::Typeface::Ptr interSemiTf()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, (size_t) BinaryData::InterSemiBold_ttfSize);
    return tf;
}

juce::String noteName (float hz)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (hz <= 0.0f) return {};
    const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

juce::String shortFreq (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + "k" : juce::String (juce::roundToInt (hz));
}
} // namespace

juce::Font ui::regular (float size) { return juce::Font (juce::FontOptions (interRegularTf()).withHeight (size)); }
juce::Font ui::semi (float size)    { return juce::Font (juce::FontOptions (interSemiTf()).withHeight (size)); }
juce::Font ui::caps (float size)    { return semi (size).withExtraKerningFactor (0.09f); }

// ============================================================================
//  Look & feel
// ============================================================================
VeloursLookAndFeel::VeloursLookAndFeel()
{
    interRegular = interRegularTf();
    interSemi = interSemiTf();

    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.35f));
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::arrowColourId, ui::dim);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::headerTextColourId, ui::dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::raisedHi);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::text);
    setColour (juce::TextEditor::backgroundColourId, ui::raised);
    setColour (juce::TextEditor::textColourId, ui::text);
    setColour (juce::TextEditor::outlineColourId, ui::line);
    setColour (juce::TextEditor::focusedOutlineColourId, ui::accent);
    setColour (juce::TextEditor::highlightColourId, ui::accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, ui::text);
    setColour (juce::AlertWindow::backgroundColourId, ui::panel);
    setColour (juce::AlertWindow::textColourId, ui::text);
    setColour (juce::AlertWindow::outlineColourId, ui::line);
    setColour (juce::TextButton::buttonColourId, ui::raised);
    setColour (juce::TextButton::textColourOffId, ui::text);
}

juce::Typeface::Ptr VeloursLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() ? interSemi : interRegular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

juce::Font VeloursLookAndFeel::getLabelFont (juce::Label& l)
{
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
        return ui::regular (12.5f);
    return ui::regular (12.0f);
}

juce::Font VeloursLookAndFeel::getComboBoxFont (juce::ComboBox&) { return ui::regular (12.5f); }
juce::Font VeloursLookAndFeel::getPopupMenuFont() { return ui::regular (13.5f); }

void VeloursLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (ui::panel);
    g.setColour (ui::line);
    g.drawRect (0, 0, w, h);
}

juce::Rectangle<int> VeloursLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (tip, ui::regular (12.5f), ui::text);
    juce::TextLayout tl;
    tl.createLayout (s, 300.0f);
    const int w = (int) tl.getWidth() + 18, h = (int) tl.getHeight() + 12;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
}

void VeloursLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tip, int w, int h)
{
    g.setColour (ui::raised);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 5.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 5.0f, 1.0f);
    juce::AttributedString s;
    s.append (tip, ui::regular (12.5f), ui::text);
    juce::TextLayout tl;
    tl.createLayout (s, (float) w - 18.0f);
    tl.draw (g, { 9.0f, 6.0f, (float) w - 18.0f, (float) h - 12.0f });
}

void VeloursLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging)
{
    g.setColour ((over || dragging) ? ui::dim : ui::faint);
    for (float i : { 0.0f, 4.0f, 8.0f })
        g.drawLine ((float) w - 3.0f - i, (float) h - 2.0f, (float) w - 2.0f, (float) h - 3.0f - i, 1.2f);
}

void VeloursLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? ui::raisedHi : ui::raised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    juce::Path chevron;
    const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (ui::dim);
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void VeloursLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

juce::Label* VeloursLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (juce::Label::textColourId, ui::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::backgroundColourId, ui::raised);
    l->setColour (juce::TextEditor::outlineColourId, ui::line);
    l->setColour (juce::TextEditor::focusedOutlineColourId, ui::accent);
    l->setFont (s.getProperties()["bar"] ? ui::regular (12.0f) : ui::regular (12.5f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void VeloursLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const bool useAccent = s.getProperties()["accent"];
    const auto valueCol = useAccent ? ui::accent : ui::text.withAlpha (0.88f);
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcW = juce::jlimit (2.0f, 4.5f, r * 0.075f);
    const float arcR = r - arcW * 0.5f;
    const juce::PathStrokeType stroke (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (ui::line);
    g.strokePath (track, stroke);

    const bool bipolar = s.getMinimum() < 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (s.isEnabled() ? valueCol : ui::faint);
        g.strokePath (val, stroke);
    }

    // corps du potard : disque mat
    const float bodyR = r - arcW * 2.6f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c.translated (0.0f, bodyR * 0.06f)));
    g.setGradientFill (juce::ColourGradient (ui::raisedHi, c.x, c.y - bodyR, ui::raised.darker (0.15f), c.x, c.y + bodyR, false));
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c));
    g.setColour (ui::line.brighter (0.15f));
    g.drawEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c), 1.0f);

    // index
    g.setColour (s.isEnabled() ? ui::text : ui::faint);
    g.drawLine ({ c.getPointOnCircumference (bodyR * 0.30f, angle), c.getPointOnCircumference (bodyR * 0.82f, angle) },
                juce::jmax (1.6f, r * 0.045f));
}

void VeloursLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    if (style == juce::Slider::LinearBar)
    {
        // boîte de valeur (barre de bande) : fond, remplissage discret
        auto box = r.reduced (0.5f);
        g.setColour (s.isMouseOverOrDragging() ? ui::raisedHi : ui::raised);
        g.fillRoundedRectangle (box, 6.0f);
        const bool bipolar = s.getMinimum() < 0.0;
        const float mid = box.getX() + box.getWidth() * (float) s.valueToProportionOfLength (0.0);
        const float px = juce::jlimit (box.getX(), box.getRight(), pos);
        auto fill = bipolar ? juce::Rectangle<float> (juce::jmin (mid, px), box.getY(), std::abs (px - mid), box.getHeight())
                            : box.withWidth (px - box.getX());
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (box, 6.0f);
        g.reduceClipRegion (clip);
        g.setColour (ui::accent.withAlpha (0.12f));
        g.fillRect (fill);
        g.restoreState();
        g.setColour (ui::line);
        g.drawRoundedRectangle (box, 6.0f, 1.0f);
        return;
    }

    // glissière horizontale fine
    const float cy = r.getCentreY();
    auto track = juce::Rectangle<float> (r.getX() + 6.0f, cy - 2.0f, r.getWidth() - 12.0f, 4.0f);
    g.setColour (ui::line);
    g.fillRoundedRectangle (track, 2.0f);
    const float px = juce::jlimit (track.getX(), track.getRight(), pos);
    const bool bipolar = s.getMinimum() < 0.0;
    const float from = bipolar ? track.getX() + track.getWidth() * (float) s.valueToProportionOfLength (0.0) : track.getX();
    g.setColour (ui::text.withAlpha (0.85f));
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (from, px), track.getY(), std::abs (px - from), track.getHeight()), 2.0f);
    g.setColour (ui::text);
    g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ px, cy }));
    g.setColour (ui::window);
    g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ px, cy }), 1.5f);
}

void VeloursLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    if (b.getProperties()["segment"])
    {
        if (on)
        {
            g.setColour (ui::text);
            g.fillRoundedRectangle (r.reduced (2.0f), 5.0f);
        }
        else if (over)
        {
            g.setColour (ui::raisedHi);
            g.fillRoundedRectangle (r.reduced (2.0f), 5.0f);
        }
        return;
    }

    const bool selectable = b.getProperties()["selectable"];
    g.setColour (selectable && on ? ui::text : (down ? ui::line : (over ? ui::raisedHi : ui::raised)));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void VeloursLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const bool filled = (b.getProperties()["segment"] || b.getProperties()["selectable"]) && on;
    g.setColour (! b.isEnabled() ? ui::faint : (filled ? ui::ink : (b.getProperties()["segment"] ? ui::dim : ui::text)));
    g.setFont (ui::caps (11.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void VeloursLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (over ? ui::raisedHi : ui::raised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? ui::accent.withAlpha (0.8f) : ui::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    const juce::Point<float> led (r.getX() + 13.0f, r.getCentreY());
    if (on)
    {
        g.setColour (ui::accent.withAlpha (0.28f));
        g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (led));
        g.setColour (ui::accent);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (led));
    }
    else
    {
        g.setColour (ui::faint);
        g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (led), 1.2f);
    }
    g.setColour (on ? ui::text : ui::dim);
    g.setFont (ui::caps (11.0f));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (25.0f).withTrimmedRight (6.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);
}

// ============================================================================
//  Graphe de réduction
// ============================================================================
static constexpr float fMin = 20.0f, fMax = 20000.0f;
static constexpr float sensRange = 18.0f;    // ± dB de la courbe de profondeur
static constexpr float redRange = 30.0f;     // dB de réduction affichés

SpectrumDisplay::SpectrumDisplay (VeloursProcessor& p) : proc (p)
{
    startTimerHz (60);
}

void SpectrumDisplay::resized()
{
    plot = getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (20.0f);
    const int w = juce::jmax (1, (int) plot.getWidth());
    colIn.assign ((size_t) w, -200.0f);
    colRed.assign ((size_t) w, 0.0f);
    colHold.assign ((size_t) w, 0.0f);
    colLo.assign ((size_t) w, 0);
    colHi.assign ((size_t) w, 0);
    scratchIn.assign ((size_t) w, -200.0f);
    scratchRed.assign ((size_t) w, 0.0f);
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
    return plot.getY() + plot.getHeight() * (0.47f - 0.36f * juce::jlimit (-sensRange, sensRange, db) / sensRange);
}

float SpectrumDisplay::sensForY (float y) const
{
    return juce::jlimit (-12.0f, 12.0f, (0.47f - (y - plot.getY()) / plot.getHeight()) / 0.36f * sensRange);
}

float SpectrumDisplay::yForSpec (float db, float f) const
{
    const float tilted = db + 4.5f * std::log2 (juce::jmax (1.0f, f) / 1000.0f);
    const float t = (tilted - 0.0f) / -96.0f;
    return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.05f, t);
}

float SpectrumDisplay::yForRed (float db) const
{
    return plot.getY() + 2.0f + plot.getHeight() * 0.70f * juce::jlimit (0.0f, 1.0f, db / redRange);
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
            sensCurve[c][(size_t) x] = bands::totalDb (bandsNow.data(), params::numBands, freqForX (plot.getX() + (float) x), c, nc, ms);
    if (nc > 1)
        for (int x = 0; x < w; ++x)
            if (std::abs (sensCurve[0][(size_t) x] - sensCurve[1][(size_t) x]) > 0.05f) { curvesDiffer = true; break; }
}

void SpectrumDisplay::timerCallback()
{
    const auto b = proc.readBands();
    const bool ms = proc.apvts.getRawParameterValue ("stereo")->load() > 0.5f;
    if (b != bandsNow || curveChannels != proc.processChannels.load() || ms != curveMS)
    {
        bandsNow = b;
        rebuildCurves();
        if (selected >= 0 && ! bandsNow[(size_t) selected].on)
            selectBand (-1);
    }

    // pas de temps réel : le lissage ne dépend pas de la cadence du minuteur
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = lastTick > 0.0 ? (float) juce::jlimit (0.002, 0.1, now - lastTick) : 1.0f / 60.0f;
    lastTick = now;

    auto& ui = proc.engine.ui;
    const int w = (int) colIn.size();
    bool fresh = false;
    std::array<float, velours::Engine::maxTracked> tHz {}, tDb {};
    int tN = 0;
    {
        const juce::SpinLock::ScopedLockType sl (ui.lock);
        if (ui.bins > 0 && ui.counter != lastCounter)
        {
            fresh = true;
            lastCounter = ui.counter;
            tN = ui.numTracked;
            tHz = ui.trackedHz;
            tDb = ui.trackedDb;
            if (ui.bins != mappedBins || ! juce::exactlyEqual (ui.binHz, mappedBinHz))
            {
                mappedBins = ui.bins;
                mappedBinHz = ui.binHz;
                for (int x = 0; x < w; ++x)
                {
                    int lo = (int) std::floor (freqForX (plot.getX() + (float) x - 0.5f) / ui.binHz);
                    int hi = (int) std::ceil (freqForX (plot.getX() + (float) x + 0.5f) / ui.binHz);
                    lo = juce::jlimit (1, ui.bins - 1, lo);
                    hi = juce::jlimit (lo, ui.bins - 1, hi);
                    colLo[(size_t) x] = lo;
                    colHi[(size_t) x] = hi;
                }
            }
            scratchIn.resize ((size_t) w);
            scratchRed.resize ((size_t) w);
            lastFresh = now;
            for (int x = 0; x < w; ++x)
            {
                float a = -200.0f, r = 0.0f;
                if (colHi[(size_t) x] - colLo[(size_t) x] <= 1)
                {
                    const float fb = juce::jlimit (1.0f, (float) (ui.bins - 2), freqForX (plot.getX() + (float) x) / ui.binHz);
                    const int k0 = (int) fb;
                    const float t = fb - (float) k0;
                    a = ui.inDb[(size_t) k0] + t * (ui.inDb[(size_t) k0 + 1] - ui.inDb[(size_t) k0]);
                    r = ui.redDb[(size_t) k0] + t * (ui.redDb[(size_t) k0 + 1] - ui.redDb[(size_t) k0]);
                }
                else
                {
                    for (int k = colLo[(size_t) x]; k <= colHi[(size_t) x]; ++k)
                    {
                        a = juce::jmax (a, ui.inDb[(size_t) k]);
                        r = juce::jmax (r, ui.redDb[(size_t) k]);
                    }
                }
                scratchIn[(size_t) x] = a;
                scratchRed[(size_t) x] = r;
            }
        }
    }

    // l'hôte livre l'audio par blocs : entre deux blocs on garde la dernière
    // image ; sans nouvelle image depuis 250 ms (lecture arrêtée), tout retombe
    const bool stale = now - lastFresh > 0.25 || (int) scratchIn.size() != w;
    const float k = dt * 30.0f;                       // en « images de référence » à 30 Hz
    auto coef = [k] (float c) { return 1.0f - std::pow (1.0f - c, k); };
    const float inUp = coef (0.6f), redUp = coef (0.8f), redDown = coef (0.25f);
    const float inFall = 36.0f * dt, holdFall = 7.5f * dt;
    for (int x = 0; x < w; ++x)
    {
        const float tIn = stale ? -200.0f : scratchIn[(size_t) x];
        const float tRed = stale ? 0.0f : scratchRed[(size_t) x];
        auto& a = colIn[(size_t) x];
        auto& r = colRed[(size_t) x];
        auto& h = colHold[(size_t) x];
        a = tIn > a ? a + inUp * (tIn - a) : juce::jmax (tIn, a - inFall);
        r = tRed > r ? r + redUp * (tRed - r) : r + redDown * (tRed - r);
        h = juce::jmax (r, h - holdFall);             // trace des coupes récentes (~7 dB/s)
    }

    updateTracks (tHz.data(), tDb.data(), fresh ? tN : 0, dt);
    maxCutDb = proc.apvts.getRawParameterValue ("maxcut")->load();
    repaint();
}

void SpectrumDisplay::updateTracks (const float* hz, const float* db, int n, float dt)
{
    const float k = dt * 30.0f;
    const float hzK = 1.0f - std::pow (0.5f, k), dbK = 1.0f - std::pow (0.4f, k);
    for (auto& t : tracks) t.life = juce::jmax (0.0f, t.life - 2.1f * dt);
    std::array<bool, 8> matched {};
    for (int i = 0; i < n; ++i)
    {
        if (db[i] < 1.5f) continue;
        int best = -1;
        float bestD = 1.0f / 6.0f;
        for (int j = 0; j < (int) tracks.size(); ++j)
        {
            if (tracks[(size_t) j].life <= 0.0f || matched[(size_t) j]) continue;
            const float d = std::abs (std::log2 (hz[i] / tracks[(size_t) j].hz));
            if (d < bestD) { bestD = d; best = j; }
        }
        if (best < 0)
            for (int j = 0; j < (int) tracks.size(); ++j)
                if (tracks[(size_t) j].life <= 0.0f && ! matched[(size_t) j]) { best = j; tracks[(size_t) j].hz = hz[i]; tracks[(size_t) j].db = 0.0f; break; }
        if (best < 0) continue;
        auto& t = tracks[(size_t) best];
        t.hz = std::exp2 (std::log2 (t.hz) + hzK * (std::log2 (hz[i]) - std::log2 (t.hz)));
        t.db += dbK * (db[i] - t.db);
        t.life = 1.0f;
        matched[(size_t) best] = true;
    }
}

void SpectrumDisplay::drawTracking (juce::Graphics& g)
{
    std::vector<int> live;
    for (int i = 0; i < (int) tracks.size(); ++i)
        if (tracks[(size_t) i].life > 0.05f && tracks[(size_t) i].db >= 1.5f) live.push_back (i);
    std::sort (live.begin(), live.end(), [this] (int a, int b) { return tracks[(size_t) a].db > tracks[(size_t) b].db; });
    if (live.size() > 4) live.resize (4);

    // points sur les creux suivis + fréquence discrète dessous (sans chevauchement :
    // la résonance la plus coupée garde son étiquette)
    std::vector<juce::Rectangle<float>> labelBoxes;
    for (int i : live)
    {
        const auto& t = tracks[(size_t) i];
        const float a = juce::jlimit (0.0f, 1.0f, t.life);
        const float x = xForFreq (t.hz), y = yForRed (t.db);
        g.setColour (ui::accent.withAlpha (a));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x, y }));
        const auto box = juce::Rectangle<float> (38.0f, 13.0f).withCentre ({ x, y + 12.0f });
        bool overlaps = false;
        for (const auto& b : labelBoxes) overlaps |= b.intersects (box);
        if (overlaps) continue;
        labelBoxes.push_back (box);
        g.setColour (ui::text.withAlpha (0.75f * a));
        g.setFont (ui::regular (10.5f));
        g.drawText (shortFreq (t.hz), box.expanded (6.0f, 0.0f), juce::Justification::centred);
    }

    // liste discrète en haut à droite
    auto area = juce::Rectangle<float> (plot.getRight() - 150.0f, plot.getBottom() - 28.0f - 15.0f * 4.0f, 138.0f, 14.0f);
    g.setFont (ui::caps (9.5f));
    g.setColour (ui::faint);
    g.drawText ("TRACKING", area, juce::Justification::centredRight);
    for (int i : live)
    {
        area.translate (0.0f, 15.0f);
        const auto& t = tracks[(size_t) i];
        const float a = juce::jlimit (0.35f, 1.0f, t.life);
        g.setFont (ui::regular (11.0f));
        g.setColour (ui::text.withAlpha (0.8f * a));
        g.drawText (params::freqText (t.hz), area.withWidth (62.0f), juce::Justification::centredLeft);
        g.setColour (ui::dim.withAlpha (a));
        g.drawText (noteName (t.hz), area.withX (area.getX() + 64.0f).withWidth (34.0f), juce::Justification::centredLeft);
        g.setColour (ui::accent.withAlpha (a));
        g.drawText ("-" + juce::String (t.db, 1), area, juce::Justification::centredRight);
    }
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour (ui::graphBg);
    g.fillRoundedRectangle (area, 10.0f);

    g.saveState();
    juce::Path clipPath;
    clipPath.addRoundedRectangle (area.reduced (1.0f), 9.0f);
    g.reduceClipRegion (clipPath);

    const int w = (int) colIn.size();
    auto colX = [this] (int x) { return plot.getX() + (float) x; };

    // grille
    for (float f : { 30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f, 300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
                     3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f })
    {
        g.setColour (ui::grid.withAlpha (0.6f));
        g.drawVerticalLine ((int) xForFreq (f), plot.getY(), plot.getBottom());
    }
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        g.setColour (ui::grid);
        g.drawVerticalLine ((int) xForFreq (f), plot.getY(), plot.getBottom());
    }
    for (float db : { 3.0f, 6.0f, 12.0f, 18.0f, 24.0f })
    {
        g.setColour (ui::grid);
        g.drawHorizontalLine ((int) yForRed (db), plot.getX(), plot.getRight());
        g.setColour (ui::faint);
        g.setFont (ui::regular (10.0f));
        g.drawText ("-" + juce::String ((int) db), juce::Rectangle<float> (plot.getX() + 8.0f, yForRed (db) - 13.0f, 30.0f, 12.0f),
                    juce::Justification::centredLeft);
    }

    // zones sans traitement (courbe de profondeur à -inf) : assombries
    {
        const int nc = juce::jmax (1, curveChannels);
        for (int x = 0; x < w; ++x)
        {
            float wMax = 0.0f;
            for (int c = 0; c < juce::jmin (2, nc); ++c)
                wMax = juce::jmax (wMax, bands::weightFromDb (sensCurve[c][(size_t) x]));
            if (wMax < 0.5f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.32f * (1.0f - wMax * 2.0f)));
                g.fillRect (colX (x), plot.getY(), 1.0f, plot.getHeight());
            }
        }
    }

    if (w > 1)
    {
        // spectre d'entrée, très discret
        juce::Path in;
        in.startNewSubPath (colX (0), plot.getBottom());
        for (int x = 0; x < w; ++x)
            in.lineTo (colX (x), yForSpec (colIn[(size_t) x], freqForX (colX (x))));
        in.lineTo (colX (w - 1), plot.getBottom());
        in.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillPath (in);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.strokePath (in, juce::PathStrokeType (1.0f));

        // zone au-delà de MAX CUT
        if (maxCutDb < redRange - 0.05f)
        {
            const float ym = yForRed (maxCutDb);
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRect (juce::Rectangle<float> (plot.getX(), ym, plot.getWidth(), plot.getBottom() - ym));
            juce::Path ml, dashed;
            ml.startNewSubPath (plot.getX(), ym);
            ml.lineTo (plot.getRight(), ym);
            const float dashes[] = { 4.0f, 4.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, ml, dashes, 2);
            g.setColour (ui::dim.withAlpha (0.6f));
            g.fillPath (dashed);
            g.setFont (ui::caps (9.5f));
            g.drawText ("MAX CUT", juce::Rectangle<float> (plot.getX() + 40.0f, ym + 3.0f, 80.0f, 12.0f), juce::Justification::centredLeft);
        }

        // trace des coupes récentes
        juce::Path hold;
        for (int x = 0; x < w; ++x)
        {
            const float y = yForRed (colHold[(size_t) x]);
            if (x == 0) hold.startNewSubPath (colX (x), y); else hold.lineTo (colX (x), y);
        }
        g.setColour (ui::accent.withAlpha (0.28f));
        g.strokePath (hold, juce::PathStrokeType (1.0f));

        // réduction en cours : aplat qui descend du haut
        juce::Path red;
        red.startNewSubPath (colX (0), plot.getY());
        for (int x = 0; x < w; ++x)
            red.lineTo (colX (x), yForRed (colRed[(size_t) x]));
        red.lineTo (colX (w - 1), plot.getY());
        red.closeSubPath();
        g.setGradientFill (juce::ColourGradient (ui::accent.withAlpha (0.12f), 0.0f, plot.getY(),
                                                 ui::accent.withAlpha (0.42f), 0.0f, yForRed (14.0f), false));
        g.fillPath (red);
        juce::Path redLine;
        for (int x = 0; x < w; ++x)
        {
            const float y = yForRed (colRed[(size_t) x]);
            if (x == 0) redLine.startNewSubPath (colX (x), y); else redLine.lineTo (colX (x), y);
        }
        g.setColour (ui::accent);
        g.strokePath (redLine, juce::PathStrokeType (1.7f));
    }

    drawTracking (g);

    // courbe de profondeur
    g.setColour (ui::text.withAlpha (0.10f));
    g.drawHorizontalLine ((int) yForSens (0.0f), plot.getX(), plot.getRight());
    for (int c = (curvesDiffer ? 1 : 0); c >= 0; --c)
    {
        juce::Path s;
        for (int x = 0; x < w; ++x)
        {
            const float y = yForSens (juce::jmax (-sensRange, sensCurve[c][(size_t) x]));
            if (x == 0) s.startNewSubPath (colX (x), y); else s.lineTo (colX (x), y);
        }
        if (curvesDiffer && c == 1)
        {
            juce::Path dashed;
            const float dashes[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.4f).createDashedStroke (dashed, s, dashes, 2);
            g.setColour (ui::text.withAlpha (0.45f));
            g.fillPath (dashed);
        }
        else
        {
            g.setColour (ui::text.withAlpha (0.85f));
            g.strokePath (s, juce::PathStrokeType (1.6f));
        }
    }

    // nœuds
    for (int b = 0; b < params::numBands; ++b)
    {
        const auto& band = bandsNow[(size_t) b];
        if (! band.on) continue;
        const auto p = nodePos (b);
        const bool sel = b == selected, hov = b == hover || b == dragging;
        const float r = 8.0f;
        if (sel)
        {
            g.setColour (ui::accent);
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f + 8.0f, r * 2.0f + 8.0f).withCentre (p), 2.0f);
        }
        else if (hov)
        {
            g.setColour (ui::text.withAlpha (0.35f));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f + 8.0f, r * 2.0f + 8.0f).withCentre (p), 1.5f);
        }
        if (band.bypass)
        {
            g.setColour (ui::graphBg);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
            g.setColour (ui::text.withAlpha (0.6f));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), 1.4f);
            g.setFont (ui::semi (10.5f));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), juce::Justification::centred);
        }
        else
        {
            g.setColour (ui::text);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
            g.setColour (ui::ink);
            g.setFont (ui::semi (10.5f));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), juce::Justification::centred);
        }
        if (band.focus != params::focusAll)
        {
            g.setColour (ui::dim);
            g.setFont (ui::semi (9.0f));
            g.drawText (juce::String ("ALRMS").substring (band.focus, band.focus + 1),
                        juce::Rectangle<float> (14.0f, 11.0f).withCentre (p.translated (0.0f, -r - 9.0f)), juce::Justification::centred);
        }
    }

    // fréquence sous la souris
    if (showHover && dragging < 0)
    {
        g.setColour (ui::text.withAlpha (0.08f));
        g.drawVerticalLine ((int) hoverPos.x, plot.getY(), plot.getBottom());
        g.setColour (ui::dim);
        g.setFont (ui::regular (10.5f));
        g.drawText (params::freqText (freqForX (hoverPos.x)) + "  " + noteName (freqForX (hoverPos.x)),
                    juce::Rectangle<float> (100.0f, 14.0f).withCentre ({ hoverPos.x, plot.getBottom() - 10.0f }), juce::Justification::centred);
    }

    // BAND LISTEN en cours
    const int lb = proc.listenBand.load();
    if (lb >= 0)
    {
        const juce::String t = "LISTENING TO BAND " + juce::String (lb + 1);
        const auto font = ui::caps (10.5f);
        auto box = juce::Rectangle<float> (24.0f + juce::GlyphArrangement::getStringWidth (font, t), 22.0f)
                       .withCentre ({ plot.getCentreX(), plot.getY() + 18.0f });
        g.setColour (ui::accent);
        g.fillRoundedRectangle (box, 11.0f);
        g.setColour (ui::ink);
        g.setFont (font);
        g.drawText (t, box, juce::Justification::centred);
    }

    g.restoreState();

    // étiquettes de fréquence
    g.setColour (ui::faint);
    g.setFont (ui::regular (10.5f));
    const std::pair<float, const char*> labels[] = { { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
                                                     { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" } };
    for (const auto& l : labels)
        g.drawText (l.second, juce::Rectangle<float> (40.0f, 16.0f).withCentre ({ xForFreq (l.first), plot.getBottom() + 10.0f }),
                    juce::Justification::centred);

    g.setColour (ui::line);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);
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
        if (b >= 0) { selectBand (b); showBandMenu (b); }
        else showAddMenu (e.position);
        return;
    }
    selectBand (b);
    dragging = b;
    if (b >= 0)
    {
        proc.undoManager.beginNewTransaction();
        for (auto* what : { "freq", "gain" })
            if (auto* p = bandParam (b, what)) p->beginChangeGesture();
        if (proc.bandListenOnDrag.load() || e.mods.isAltDown())
            proc.listenBand = b;
    }
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
    proc.listenBand = -1;
}

void SpectrumDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = hitBand (e.position);
    proc.undoManager.beginNewTransaction();
    if (b >= 0)
    {
        // comme soothe3 : double-clic = bande active / contournée
        setBandValue (b, "byp", bandsNow[(size_t) b].bypass ? 0.0f : 1.0f);
        return;
    }
    if (plot.contains (e.position))
        addBand (freqForX (e.position.x), e.mods.isCommandDown() ? 0.0f : sensForY (e.position.y),
                 e.mods.isCommandDown() ? params::bandPass : params::bell);
}

void SpectrumDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int b = hitBand (e.position);
    if (b < 0) b = selected;
    if (b < 0 || ! bandsNow[(size_t) b].on) return;
    const float speed = e.mods.isShiftDown() ? 0.3f : 1.5f;
    const float q = bandsNow[(size_t) b].q * std::pow (2.0f, wheel.deltaY * (wheel.isReversed ? -speed : speed));
    setBandValue (b, "q", juce::jlimit (0.2f, 10.0f, q));
}

void SpectrumDisplay::addBand (float freq, float gain, int type)
{
    for (int b = 0; b < params::numBands; ++b)
        if (! bandsNow[(size_t) b].on)
        {
            const bool noGain = ! usesGain (type);
            setBandValue (b, "type", (float) type);
            setBandValue (b, "freq", freq);
            setBandValue (b, "gain", noGain ? 0.0f : gain);
            setBandValue (b, "q", type == params::lowCut || type == params::highCut ? 0.71f : 1.0f);
            setBandValue (b, "focus", (float) params::focusAll);
            setBandValue (b, "byp", 0.0f);
            setBandValue (b, "on", 1.0f);
            bandsNow = proc.readBands();
            rebuildCurves();
            selectBand (b);
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
    juce::PopupMenu focusMenu;
    for (int f = 0; f < params::bandFocusNames.size(); ++f)
        focusMenu.addItem (200 + f, params::bandFocusNames[f], true, b.focus == f);
    m.addSeparator();
    m.addSubMenu ("Focus", focusMenu);
    m.addItem (300, "Bypass band", true, b.bypass);
    m.addItem (301, "Reset depth to 0 dB");
    m.addSeparator();
    m.addItem (400, "Delete band");

    juce::Component::SafePointer<SpectrumDisplay> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, band] (int r)
    {
        if (safe == nullptr || r == 0) return;
        safe->proc.undoManager.beginNewTransaction();
        if (r >= 100 && r < 200) safe->setBandValue (band, "type", (float) (r - 100));
        else if (r >= 200 && r < 300) safe->setBandValue (band, "focus", (float) (r - 200));
        else if (r == 300) safe->setBandValue (band, "byp", safe->bandsNow[(size_t) band].bypass ? 0.0f : 1.0f);
        else if (r == 301) safe->setBandValue (band, "gain", 0.0f);
        else if (r == 400) { safe->setBandValue (band, "on", 0.0f); safe->selectBand (-1); }
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
        if (safe != nullptr && r >= 100 && r < 200)
        {
            safe->proc.undoManager.beginNewTransaction();
            safe->addBand (f, gain, r - 100);
        }
    });
}

// ============================================================================
//  Potards, glissières, sélecteurs
// ============================================================================
// unité affichée après la valeur (« 5.0 ms », « 50% », « +1.5 dB »)
static juce::String unitSuffix (const juce::RangedAudioParameter& p)
{
    const auto label = p.getLabel();
    if (label.isEmpty() || p.getParameterID() == "focus") return {};
    return label == "%" ? label : " " + label;
}

Knob::Knob()
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible (slider);
}

void Knob::attach (VeloursProcessor& p, const juce::String& id, const juce::String& text, Size sz, const juce::String& tip, bool useAccent)
{
    title = text;
    size = sz;
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    if (auto* prm = p.apvts.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
        slider.setTextValueSuffix (unitSuffix (*prm));
        slider.updateText();
    }
    slider.getProperties().set ("accent", useAccent);
    slider.setTooltip (tip);
    auto* um = &p.undoManager;
    slider.onDragStart = [um] { um->beginNewTransaction(); };
    resized();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (size == Size::big ? 18 : 15);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmax (40, juce::jmin (90, getWidth())), size == Size::big ? 20 : 18);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (ui::dim);
    g.setFont (ui::caps (size == Size::big ? 11.0f : 9.5f));
    g.drawText (title, getLocalBounds().removeFromTop (size == Size::big ? 16 : 13), juce::Justification::centred);
}

HSlider::HSlider()
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    addAndMakeVisible (slider);
}

void HSlider::attach (VeloursProcessor& p, const juce::String& id, const juce::String& text, const juce::String& tip)
{
    title = text;
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    if (auto* prm = p.apvts.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
        slider.setTextValueSuffix (unitSuffix (*prm));
        slider.updateText();
    }
    slider.setTooltip (tip);
    auto* um = &p.undoManager;
    slider.onDragStart = [um] { um->beginNewTransaction(); };
}

void HSlider::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (52);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 18);
    slider.setBounds (r);
}

void HSlider::paint (juce::Graphics& g)
{
    g.setColour (ui::dim);
    g.setFont (ui::caps (10.0f));
    g.drawText (title, getLocalBounds().withWidth (50), juce::Justification::centredLeft);
}

Segments::Segments (juce::RangedAudioParameter& param, const juce::StringArray& labels, juce::UndoManager* um)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->getProperties().set ("segment", true);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        b->onClick = [this, i, um]
        {
            if (um != nullptr) um->beginNewTransaction();
            if (attachment) attachment->setValueAsCompleteGesture ((float) i);
        };
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

void Segments::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::raised);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

void Segments::resized()
{
    auto r = getLocalBounds();
    const int n = buttons.size();
    for (int i = 0; i < n; ++i)
    {
        const int w = r.getWidth() / (n - i);
        buttons[i]->setBounds (r.removeFromLeft (w));
    }
}

void LevelReadout::timerCallback()
{
    const float o = proc.engine.overallReductionDb.load();
    const float p = proc.engine.peakReductionDb.load();
    const float m = proc.engine.makeupDbOut.load();
    const bool gm = proc.apvts.getRawParameterValue ("gainMatch")->load() > 0.5f
                    && proc.apvts.getRawParameterValue ("delta")->load() < 0.5f;
    overall += (o - overall) * 0.3f;
    peak += (p - peak) * (p > peak ? 0.6f : 0.12f);
    makeup += (m - makeup) * 0.3f;
    gainMatchOn = gm;
    repaint();
}

void LevelReadout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::dim);
    g.setFont (ui::caps (9.5f));
    auto titleRow = r.removeFromTop (13.0f);
    g.drawText ("REDUCTION", titleRow, juce::Justification::centredLeft);
    if (gainMatchOn)
    {
        g.setColour (ui::text.withAlpha (0.7f));
        g.setFont (ui::regular (10.5f));
        g.drawText ("+" + juce::String (juce::jmax (0.0f, makeup), 1), titleRow, juce::Justification::centredRight);
    }
    auto row = r.removeFromTop (20.0f);
    g.setColour (ui::accent);
    g.setFont (ui::semi (16.0f));
    g.drawText ((overall > 0.05f ? "-" : "") + juce::String (overall, 1) + " dB", row, juce::Justification::centredLeft);
    auto bar = r.removeFromTop (6.0f).withTrimmedTop (2.0f);
    g.setColour (ui::line);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (ui::accent.withAlpha (0.8f));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, peak / 24.0f)), 2.0f);
}

// ============================================================================
//  Barre de bande
// ============================================================================
BandStrip::BandStrip (VeloursProcessor& p) : proc (p)
{
    for (auto* b : { &power, &listen })
    {
        b->getProperties().set ("selectable", true);
        b->setClickingTogglesState (true);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addChildComponent (b);
    }
    power.setTooltip ("Band on / bypassed (double-click a node does the same)");
    listen.setTooltip ("Hear only what is removed in this band's area");
    listen.onClick = [this] { proc.listenBand = listen.getToggleState() ? band : -1; };
    remove.setTooltip ("Delete band");
    remove.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    remove.onClick = [this]
    {
        if (band < 0) return;
        proc.undoManager.beginNewTransaction();
        if (auto* prm = proc.apvts.getParameter (params::bandId (band, "on")))
        {
            prm->beginChangeGesture(); prm->setValueNotifyingHost (0.0f); prm->endChangeGesture();
        }
        if (onDelete) onDelete();
    };
    addChildComponent (remove);

    shape.addItemList (params::bandTypeNames, 1);
    focus.addItemList (params::bandFocusNames, 1);
    shape.setTooltip ("Band shape");
    focus.setTooltip ("Stereo focus of this band (L/R or M/S depending on the stereo mode)");
    addChildComponent (shape);
    addChildComponent (focus);

    for (auto* s : { &freq, &gain, &q })
    {
        s->setSliderStyle (juce::Slider::LinearBar);
        s->getProperties().set ("bar", true);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        s->onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addChildComponent (s);
    }
    freq.setTooltip ("Frequency (drag, or double-click to type)");
    gain.setTooltip ("Depth: + boosts resonance suppression in this area, - reduces it");
    gain.setTextValueSuffix (" dB");
    q.setTooltip ("Width (Q). Mouse wheel on the node works too");
}

BandStrip::~BandStrip()
{
    if (listen.getToggleState() && proc.listenBand.load() == band) proc.listenBand = -1;
}

void BandStrip::setBand (int b)
{
    if (b == band) return;
    if (listen.getToggleState()) { listen.setToggleState (false, juce::dontSendNotification); proc.listenBand = -1; }
    band = b;
    freqAtt.reset(); gainAtt.reset(); qAtt.reset(); shapeAtt.reset(); focusAtt.reset(); bypAtt.reset();
    const bool show = band >= 0;
    for (juce::Component* c : { (juce::Component*) &power, (juce::Component*) &listen, (juce::Component*) &remove,
                                (juce::Component*) &shape, (juce::Component*) &focus, (juce::Component*) &freq,
                                (juce::Component*) &gain, (juce::Component*) &q })
        c->setVisible (show);
    if (show)
    {
        auto& s = proc.apvts;
        freqAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, params::bandId (band, "freq"), freq);
        gainAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, params::bandId (band, "gain"), gain);
        qAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, params::bandId (band, "q"), q);
        shapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, params::bandId (band, "type"), shape);
        focusAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, params::bandId (band, "focus"), focus);
        q.textFromValueFunction = [] (double v) { return "Q " + juce::String (v, 2); };
        q.updateText();
        if (auto* bp = s.getParameter (params::bandId (band, "byp")))
        {
            bypAtt = std::make_unique<juce::ParameterAttachment> (*bp, [this] (float v)
            {
                power.setToggleState (v < 0.5f, juce::dontSendNotification);
                power.setButtonText (v < 0.5f ? "ON" : "OFF");
            }, &proc.undoManager);
            bypAtt->sendInitialUpdate();
            power.onClick = [this]
            {
                proc.undoManager.beginNewTransaction();
                if (bypAtt) bypAtt->setValueAsCompleteGesture (power.getToggleState() ? 0.0f : 1.0f);
            };
        }
    }
    resized();
    repaint();
}

void BandStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 7);
    r.removeFromLeft (60);
    auto place = [&r] (juce::Component& c, int w) { c.setBounds (r.removeFromLeft (w)); r.removeFromLeft (5); };
    place (power, 40);
    place (listen, 58);
    place (shape, 100);
    place (freq, 88);
    place (gain, 80);
    place (q, 66);
    place (focus, 66);
    remove.setBounds (r.removeFromLeft (juce::jmax (24, r.getWidth())));
}

void BandStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
    if (band >= 0)
    {
        auto badge = juce::Rectangle<float> (8.0f, r.getCentreY() - 11.0f, 54.0f, 22.0f);
        g.setColour (ui::text);
        g.fillRoundedRectangle (badge, 11.0f);
        g.setColour (ui::ink);
        g.setFont (ui::caps (10.5f));
        g.drawText ("BAND " + juce::String (band + 1), badge, juce::Justification::centred);
    }
    else
    {
        g.setColour (ui::dim);
        g.setFont (ui::regular (12.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("Click a node to edit it   \xc2\xb7   Double-click the graph to add a band   \xc2\xb7   "
                                                          "Alt+drag a node to listen   \xc2\xb7   Right-click for options")),
                    r.reduced (14.0f, 0.0f), juce::Justification::centredLeft);
    }
}

// ============================================================================
//  Sauvegarde de preset (panneau intégré)
// ============================================================================
SaveOverlay::SaveOverlay()
{
    setVisible (false);
    setWantsKeyboardFocus (false);
    name.setFont (ui::regular (14.0f));
    name.setJustification (juce::Justification::centredLeft);
    name.setIndents (10, 0);
    name.setInputRestrictions (60);
    name.setSelectAllWhenFocused (true);
    name.onReturnKey = [this] { commit(); };
    name.onEscapeKey = [this] { hide(); };
    name.onTextChange = [this] { updateHint(); };
    addAndMakeVisible (name);

    save.getProperties().set ("selectable", true);
    save.setToggleState (true, juce::dontSendNotification);   // bouton principal, plein
    for (auto* b : { &save, &cancel })
    {
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (b);
    }
    save.onClick = [this] { commit(); };
    cancel.onClick = [this] { hide(); };
}

void SaveOverlay::show (const juce::String& initialName)
{
    name.setText (initialName, juce::dontSendNotification);
    hintIsError = false;
    updateHint();
    setVisible (true);
    toFront (false);
    name.grabKeyboardFocus();
    name.selectAll();
}

void SaveOverlay::hide()
{
    setVisible (false);
}

void SaveOverlay::updateHint()
{
    const auto clean = juce::File::createLegalFileName (name.getText().trim());
    hintIsError = false;
    if (clean.isEmpty())
        hint = "Type a name";
    else if (presets::userFolder().getChildFile (clean + ".velours").existsAsFile())
        hint = "Replaces the existing user preset \"" + clean + "\"";
    else
        hint = "Saved in the user presets folder";
    save.setEnabled (clean.isNotEmpty());
    repaint();
}

void SaveOverlay::commit()
{
    if (! save.isEnabled()) return;
    if (onSave != nullptr && onSave (name.getText()))
        hide();
    else
    {
        hint = "Could not write the preset file";
        hintIsError = true;
        repaint();
    }
}

void SaveOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! card.contains (e.getPosition()))
        hide();
}

void SaveOverlay::resized()
{
    card = getLocalBounds().withSizeKeepingCentre (380, 168);
    auto r = card.reduced (20, 18);
    r.removeFromTop (26);
    name.setBounds (r.removeFromTop (34));
    r.removeFromTop (28);
    auto buttons = r.removeFromTop (30);
    save.setBounds (buttons.removeFromRight (92));
    buttons.removeFromRight (8);
    cancel.setBounds (buttons.removeFromRight (92));
}

void SaveOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    const auto c = card.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (c.translated (0.0f, 4.0f).expanded (2.0f), 12.0f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (c, 10.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (c.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (ui::text);
    g.setFont (ui::caps (11.0f));
    g.drawText ("SAVE PRESET", card.reduced (20, 18).removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (hintIsError ? ui::accent : ui::faint);
    g.setFont (ui::regular (11.5f));
    g.drawText (hint, juce::Rectangle<int> (name.getX(), name.getBottom() + 6, name.getWidth(), 16), juce::Justification::centredLeft);
}

// ============================================================================
//  Panneau principal
// ============================================================================
Panel::Panel (VeloursProcessor& p) : proc (p), display (p), strip (p), level (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;
    auto* um = &proc.undoManager;

    addAndMakeVisible (display);
    addAndMakeVisible (strip);
    display.onSelectionChange = [this] (int b) { strip.setBand (b); };
    strip.onDelete = [this] { display.selectBand (-1); };

    depth.attach (p, "depth", "DEPTH", Knob::Size::big, "How much the resonances are reduced", true);
    detail.attach (p, "detail", "DETAIL", Knob::Size::big,
                   "Low = broad, smooth reduction of build-ups. High = narrow, deeper cuts on the most prominent resonances");
    attack.attach (p, "attack", "ATTACK", Knob::Size::medium, "How fast the cuts react (faster in the highs)");
    release.attach (p, "release", "RELEASE", Knob::Size::medium, "How fast the cuts recover. Longer = smoother, fewer artefacts");
    detailTilt.attach (p, "detailTilt", "DETAIL", Knob::Size::small, "Detail tilt: + = more detail in the highs, - = in the lows");
    attackTilt.attach (p, "timeTilt", "ATTACK", Knob::Size::small, "Attack tilt: + = faster highs, slower lows");
    releaseTilt.attach (p, "releaseTilt", "RELEASE", Knob::Size::small, "Release tilt: + = faster highs, slower lows");
    maxCut.attach (p, "maxcut", "MAX CUT", Knob::Size::small, "Hard limit on the attenuation at any frequency");
    wetTrim.attach (p, "wetTrim", "WET TRIM", Knob::Size::small, "Level of the processed signal (before Mix)");
    link.attach (p, "link", "LINK", Knob::Size::small, "100 % = both channels get the same processing");
    focus.attach (p, "focus", "FOCUS", Knob::Size::small, "Moves the processing towards L/Mid (left) or R/Side (right)");
    mix.attach (p, "mix", "MIX", "Dry / processed balance (phase aligned)");
    output.attach (p, "output", "OUTPUT", "Output gain");

    for (auto* k : { &depth, &detail, &attack, &release, &detailTilt, &attackTilt, &releaseTilt, &maxCut, &wetTrim, &link, &focus })
        addAndMakeVisible (k);
    addAndMakeVisible (mix);
    addAndMakeVisible (output);

    modeSeg = std::make_unique<Segments> (*s.getParameter ("mode"), juce::StringArray { "SOFT", "HARD" }, um);
    modeSeg->setTip ("Soft: adaptive threshold, level independent, transparent. Hard: fixed threshold, level dependent, firmer");
    stereoSeg = std::make_unique<Segments> (*s.getParameter ("stereo"), juce::StringArray { "L / R", "M / S" }, um);
    stereoSeg->setTip ("Process left/right or mid/side");
    addAndMakeVisible (*modeSeg);
    addAndMakeVisible (*stereoSeg);

    attachToggle (sidechain, "sidechain", "SIDECHAIN", "Detect resonances on the external sidechain input");
    attachToggle (delta, "delta", "DELTA", "Listen only to what is being removed");
    attachToggle (bypass, "bypass", "BYPASS", "Bypass (latency compensated)");
    attachToggle (renderUltra, "renderUltra", "RENDER IN ULTRA", "Use Ultra quality automatically when bouncing / exporting");
    attachToggle (gainMatch, "gainMatch", "GAIN MATCH",
                  "Compensates the loudness removed by the processing (perceptual weighting, slow), "
                  "so that bypass comparisons are fair. Off while Delta is on");


    addAndMakeVisible (level);
    scInfo.setFont (ui::regular (10.5f));
    scInfo.setColour (juce::Label::textColourId, ui::faint);
    scInfo.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (scInfo);

    // Presets
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &copyBtn, &undoBtn, &redoBtn, &slotA, &slotB })
    {
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (b);
    }
    for (auto* b : { &slotA, &slotB }) b->getProperties().set ("selectable", true);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePresetDialog(); };
    saveBtn.setTooltip ("Save the current settings as a user preset");
    presetBox.setTooltip ("Presets. A dot means the current settings differ from the loaded preset");
    slotA.onClick = [this] { proc.selectSlot (0); };
    slotB.onClick = [this] { proc.selectSlot (1); };
    copyBtn.onClick = [this] { proc.copyToOtherSlot(); };
    copyBtn.setTooltip ("Copy the current settings to the other slot");
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1 && id < 1000) proc.loadFactoryPreset (id - 1);
        else if (id >= 1000 && id - 1000 < userFiles.size()) proc.loadUserPreset (userFiles[id - 1000]);
        shownPreset = proc.getPresetName();
    };

    // Moteur
    resolutionBox.addItemList (params::qualityNames, 1);
    resolutionAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, "quality", resolutionBox);
    resolutionBox.setTooltip ("Frequency resolution / latency. Low Latency 21 ms, Normal 43 ms, High Res 85 ms. "
                              "Zero Latency: no delay at all (minimum-phase filtering, for tracking and live use)");
    qualityBox.addItemList (params::timeQualityNames, 1);
    qualityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, "timeQuality", qualityBox);
    qualityBox.setTooltip ("Time resolution of the processing: Normal, High (2x CPU), Ultra (4x CPU). Latency is unchanged");
    addAndMakeVisible (resolutionBox);
    addAndMakeVisible (qualityBox);

    scaleBox.addItemList ({ "75%", "100%", "125%", "150%", "175%", "200%" }, 1);
    showScale ((float) (double) proc.apvts.state.getProperty ("uiScale", 1.0));
    scaleBox.onChange = [this]
    {
        const float v[] = { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
        const int id = scaleBox.getSelectedId();
        if (id >= 1 && id <= 6 && onScaleChange) onScaleChange (v[id - 1]);
    };
    scaleBox.setTooltip ("Window size (you can also drag the bottom-right corner)");
    addAndMakeVisible (scaleBox);

    // sauvegarde : panneau intégré, au-dessus de tout
    saveOverlay.onSave = [this] (const juce::String& n)
    {
        if (! proc.saveUserPreset (n)) return false;
        refreshPresetBox();
        return true;
    };
    addChildComponent (saveOverlay);

    refreshPresetBox();
    startTimerHz (10);
}

Panel::~Panel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Panel::attachToggle (juce::ToggleButton& t, const juce::String& id, const juce::String& text, const juce::String& tip)
{
    t.setButtonText (text);
    t.setTooltip (tip);
    t.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t));
    addAndMakeVisible (t);
}

void Panel::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.getRootMenu()->addSectionHeader ("FACTORY");
    const auto& list = presets::factory();
    for (int i = 0; i < (int) list.size(); ++i)
        presetBox.addItem (list[(size_t) i].name, i + 1);
    userFiles = presets::userPresets();
    if (! userFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.getRootMenu()->addSectionHeader ("USER");
        for (int i = 0; i < userFiles.size(); ++i)
            presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), 1000 + i);
    }
    shownPreset = proc.getPresetName();
    int found = 0;
    for (int i = 0; i < (int) list.size(); ++i)
        if (shownPreset == list[(size_t) i].name) found = i + 1;
    for (int i = 0; i < userFiles.size() && found == 0; ++i)
        if (shownPreset == userFiles[i].getFileNameWithoutExtension()) found = 1000 + i;
    if (found > 0) presetBox.setSelectedId (found, juce::dontSendNotification);
    else presetBox.setText (shownPreset, juce::dontSendNotification);
}

void Panel::stepPreset (int step)
{
    juce::Array<int> ids;
    for (int i = 0; i < (int) presets::factory().size(); ++i) ids.add (i + 1);
    for (int i = 0; i < userFiles.size(); ++i) ids.add (1000 + i);
    int cur = ids.indexOf (presetBox.getSelectedId());
    if (cur < 0) cur = 0;
    const int n = ids.size();
    presetBox.setSelectedId (ids[((cur + step) % n + n) % n], juce::sendNotificationSync);
}

void Panel::savePresetDialog()
{
    saveOverlay.show (proc.getPresetName());
}

void Panel::showScale (float scale)
{
    const float v[] = { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    for (int i = 0; i < 6; ++i)
        if (std::abs (scale - v[i]) < 0.006f)
        {
            scaleBox.setSelectedId (i + 1, juce::dontSendNotification);
            return;
        }
    scaleBox.setText (juce::String (juce::roundToInt (scale * 100.0f)) + "%", juce::dontSendNotification);
}

void Panel::paintOverChildren (juce::Graphics& g)
{
    if (presetModified && ! saveOverlay.isVisible())
    {
        // pastille « modifié » dans le menu des presets, avant le chevron
        const auto c = juce::Point<float> ((float) presetBox.getRight() - 30.0f, (float) presetBox.getBounds().getCentreY());
        g.setColour (ui::accent);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c));
    }
}

void Panel::timerCallback()
{
    if (proc.getPresetName() != shownPreset)
        refreshPresetBox();

    slotA.setToggleState (proc.getActiveSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState (proc.getActiveSlot() == 1, juce::dontSendNotification);
    undoBtn.setEnabled (proc.undoManager.canUndo());
    redoBtn.setEnabled (proc.undoManager.canRedo());

    const bool modified = proc.isPresetModified();
    if (modified != presetModified)
    {
        presetModified = modified;
        repaint (presetBox.getBounds().expanded (2));
    }

    // latence affichée à côté de RESOLUTION
    juce::String lat;
    if (proc.getSampleRate() > 0.0)
    {
        const double ms = 1000.0 * proc.getLatencySamples() / proc.getSampleRate();
        lat = ms < 0.05 ? juce::String ("0 ms") : (ms < 10.0 ? juce::String (ms, 1) : juce::String (juce::roundToInt (ms))) + " ms";
    }
    if (lat != latencyText)
    {
        latencyText = lat;
        repaint (resolutionBox.getBounds().translated (0, -16).withHeight (13));
    }


    const bool scOn = proc.apvts.getRawParameterValue ("sidechain")->load() > 0.5f;
    const bool connected = proc.sidechainConnected.load();
    const juce::String t = ! scOn ? juce::String ("Detects on the main input")
                         : connected ? juce::String ("Detecting on the sidechain")
                                     : juce::String ("No sidechain routed");
    if (scInfo.getText() != t) scInfo.setText (t, juce::dontSendNotification);
    scInfo.setColour (juce::Label::textColourId, scOn && ! connected ? ui::accent : ui::faint);

    const bool stereo = proc.processChannels.load() > 1;
    for (auto* k : { &link, &focus }) { k->setEnabled (stereo); k->setAlpha (stereo ? 1.0f : 0.4f); }
}

void Panel::sectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t)
{
    g.setColour (ui::faint);
    g.setFont (ui::caps (9.5f));
    g.drawText (t, r, juce::Justification::centredLeft);
    const int tw = (int) juce::GlyphArrangement::getStringWidth (ui::caps (9.5f), t);
    g.setColour (ui::line);
    g.drawHorizontalLine (r.getCentreY(), (float) (r.getX() + tw + 8), (float) r.getRight());
}

void Panel::paint (juce::Graphics& g)
{
    g.fillAll (ui::window);

    // barre du haut
    g.setColour (ui::ink);
    g.fillRect (topArea);
    g.setColour (ui::line);
    g.drawHorizontalLine (topArea.getBottom() - 1, 0.0f, (float) baseW);

    // marque : un petit « creux » + VELOURS
    {
        const float x0 = 20.0f, cy = (float) topArea.getCentreY();
        juce::Path mark;
        mark.startNewSubPath (x0, cy - 6.0f);
        mark.cubicTo (x0 + 6.0f, cy - 6.0f, x0 + 6.0f, cy + 7.0f, x0 + 9.0f, cy + 7.0f);
        mark.cubicTo (x0 + 12.0f, cy + 7.0f, x0 + 12.0f, cy - 6.0f, x0 + 18.0f, cy - 6.0f);
        g.setColour (ui::accent);
        g.strokePath (mark, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (ui::text);
        g.setFont (ui::semi (17.0f).withExtraKerningFactor (0.22f));
        g.drawText ("VELOURS", juce::Rectangle<float> (x0 + 28.0f, cy - 12.0f, 130.0f, 24.0f), juce::Justification::centredLeft);
        g.setColour (ui::faint);
        g.setFont (ui::regular (10.5f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("by Aociz  \xc2\xb7  v")) + juce::String (JucePlugin_VersionString),
                    juce::Rectangle<float> (x0 + 150.0f, cy - 8.0f, 120.0f, 16.0f), juce::Justification::centredLeft);
    }

    // panneaux
    for (auto r : { leftArea, rightArea })
    {
        g.setColour (ui::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (ui::line);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    // titres de sections
    sectionTitle (g, { leftArea.getX() + 14, leftArea.getY() + 352, leftArea.getWidth() - 28, 14 }, "TIMING");
    sectionTitle (g, { leftArea.getX() + 14, leftArea.getY() + 480, leftArea.getWidth() - 28, 14 }, "MODE");
    sectionTitle (g, { rightArea.getX() + 14, rightArea.getY() + 12, rightArea.getWidth() - 28, 14 }, "TILT");
    sectionTitle (g, { rightArea.getX() + 14, rightArea.getY() + 124, rightArea.getWidth() - 28, 14 }, "LIMIT & TRIM");
    sectionTitle (g, { rightArea.getX() + 14, rightArea.getY() + 236, rightArea.getWidth() - 28, 14 }, "STEREO");
    sectionTitle (g, { rightArea.getX() + 14, rightArea.getY() + 448, rightArea.getWidth() - 28, 14 }, "ENGINE");

    // pied de page
    g.setColour (ui::line);
    g.drawHorizontalLine (footerArea.getY(), 0.0f, (float) baseW);
    for (int x : { 210, 770 })
        g.drawVerticalLine (x, (float) footerArea.getY() + 16.0f, (float) footerArea.getBottom() - 16.0f);
    g.setColour (ui::dim);
    g.setFont (ui::caps (9.5f));
    const auto resTitle = resolutionBox.getBounds().translated (0, -16).withHeight (13);
    g.drawText ("RESOLUTION", resTitle, juce::Justification::centredLeft);
    if (latencyText.isNotEmpty())
    {
        g.setColour (ui::faint);
        g.setFont (ui::regular (10.0f));
        g.drawText (latencyText, resTitle, juce::Justification::centredRight);
    }
    g.drawText ("QUALITY", qualityBox.getBounds().translated (0, -16).withHeight (13), juce::Justification::centredLeft);
}

void Panel::resized()
{
    topArea = { 0, 0, baseW, 48 };
    leftArea = { 12, 58, 214, 536 };
    rightArea = { baseW - 12 - 204, 58, 204, 536 };
    footerArea = { 0, 606, baseW, baseH - 606 };

    // barre du haut
    {
        const int y = 10, h = 28;
        int x = 300;
        prevBtn.setBounds (x, y, 28, h);  x += 32;
        presetBox.setBounds (x, y, 270, h); x += 274;
        nextBtn.setBounds (x, y, 28, h);  x += 36;
        saveBtn.setBounds (x, y, 58, h);
        int rx = baseW - 16;
        scaleBox.setBounds (rx - 74, y, 74, h); rx -= 74 + 16;
        redoBtn.setBounds (rx - 56, y, 56, h); rx -= 60;
        undoBtn.setBounds (rx - 56, y, 56, h); rx -= 56 + 16;
        copyBtn.setBounds (rx - 44, y, 44, h); rx -= 48;
        slotB.setBounds (rx - 30, y, 30, h); rx -= 34;
        slotA.setBounds (rx - 30, y, 30, h);
    }

    // centre : graphe + barre de bande
    const int gx = leftArea.getRight() + 10, gw = rightArea.getX() - 10 - gx;
    display.setBounds (gx, 58, gw, 486);
    strip.setBounds (gx, 550, gw, 44);

    // colonne de gauche : DEPTH, DETAIL, ATTACK / RELEASE, MODE
    {
        const auto& a = leftArea;
        depth.setBounds (a.getCentreX() - 70, a.getY() + 12, 140, 162);
        detail.setBounds (a.getCentreX() - 70, a.getY() + 182, 140, 162);
        attack.setBounds (a.getX() + 10, a.getY() + 372, 94, 100);
        release.setBounds (a.getRight() - 104, a.getY() + 372, 94, 100);
        modeSeg->setBounds (a.getX() + 14, a.getY() + 500, a.getWidth() - 28, 28);
    }

    // panneau de droite
    {
        const auto& a = rightArea;
        const int cw = (a.getWidth() - 20) / 3;
        detailTilt.setBounds (a.getX() + 10, a.getY() + 30, cw, 86);
        attackTilt.setBounds (a.getX() + 10 + cw, a.getY() + 30, cw, 86);
        releaseTilt.setBounds (a.getX() + 10 + 2 * cw, a.getY() + 30, cw, 86);
        const int hw = (a.getWidth() - 20) / 2;
        maxCut.setBounds (a.getX() + 10, a.getY() + 142, hw, 86);
        wetTrim.setBounds (a.getX() + 10 + hw, a.getY() + 142, hw, 86);
        stereoSeg->setBounds (a.getX() + 14, a.getY() + 258, a.getWidth() - 28, 26);
        link.setBounds (a.getX() + 10, a.getY() + 292, hw, 86);
        focus.setBounds (a.getX() + 10 + hw, a.getY() + 292, hw, 86);
        sidechain.setBounds (a.getX() + 14, a.getY() + 388, a.getWidth() - 28, 28);
        scInfo.setBounds (a.getX() + 16, a.getY() + 418, a.getWidth() - 28, 16);
        renderUltra.setBounds (a.getX() + 14, a.getY() + 470, a.getWidth() - 28, 28);
    }

    // pied de page
    {
        const int cy = footerArea.getCentreY();
        bypass.setBounds (16, cy - 15, 92, 30);
        delta.setBounds (114, cy - 15, 82, 30);
        mix.setBounds (224, cy - 15, 200, 30);
        output.setBounds (434, cy - 15, 200, 30);
        gainMatch.setBounds (646, cy - 15, 112, 30);
        level.setBounds (784, cy - 20, 92, 40);
        qualityBox.setBounds (baseW - 14 - 84, cy - 6, 84, 26);
        resolutionBox.setBounds (qualityBox.getX() - 8 - 112, cy - 6, 112, 26);
    }

    saveOverlay.setBounds (getLocalBounds());
}

// ============================================================================
//  Éditeur (zoom)
// ============================================================================
VeloursEditor::VeloursEditor (VeloursProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p)
{
    const float stored = (float) (double) proc.apvts.state.getProperty ("uiScale", 1.0);
    setLookAndFeel (&panel.getLookAndFeel());   // infobulles au style Velours
    addAndMakeVisible (panel);
    panel.onScaleChange = [this] (float s) { applyScale (s); };

    // fenêtre redimensionnable, proportions fixes (coin en bas à droite ou bord de la fenêtre hôte)
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (Panel::baseW * minScale), juce::roundToInt (Panel::baseH * minScale),
                     juce::roundToInt (Panel::baseW * maxScale), juce::roundToInt (Panel::baseH * maxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) Panel::baseW / (double) Panel::baseH);
    applyScale (stored);
}

VeloursEditor::~VeloursEditor()
{
    setLookAndFeel (nullptr);
}

void VeloursEditor::applyScale (float s)
{
    s = juce::jlimit (minScale, maxScale, std::isfinite (s) ? s : 1.0f);
    setSize (juce::roundToInt (Panel::baseW * s), juce::roundToInt (Panel::baseH * s));
}

void VeloursEditor::resized()
{
    scale = juce::jlimit (minScale, maxScale, (float) getWidth() / (float) Panel::baseW);
    panel.setTransform (juce::AffineTransform::scale (scale));
    panel.setBounds (0, 0, Panel::baseW, Panel::baseH);
    proc.apvts.state.setProperty ("uiScale", scale, nullptr);
    panel.showScale (scale);
}

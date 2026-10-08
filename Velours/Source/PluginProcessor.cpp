#include "PluginProcessor.h"
#include "PluginEditor.h"

VeloursProcessor::VeloursProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "VELOURS", params::createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));

    pDepth = raw ("depth");         pDetail = raw ("detail");
    pAttack = raw ("attack");       pRelease = raw ("release");
    pMaxCut = raw ("maxcut");       pMode = raw ("mode");
    pDetailTilt = raw ("detailTilt"); pTimeTilt = raw ("timeTilt");
    pStereo = raw ("stereo");       pLink = raw ("link");
    pSidechain = raw ("sidechain"); pMix = raw ("mix");
    pWetTrim = raw ("wetTrim");     pOutput = raw ("output");
    pDelta = raw ("delta");         pBypass = raw ("bypass");
    pQuality = raw ("quality");
    pReleaseTilt = raw ("releaseTilt");
    pTimeQuality = raw ("timeQuality");
    pRenderUltra = raw ("renderUltra");
    pFocus = raw ("focus");
    pGainMatch = raw ("gainMatch");

    for (int b = 0; b < params::numBands; ++b)
    {
        auto& r = bandRaw[(size_t) b];
        r.on    = apvts.getRawParameterValue (params::bandId (b, "on"));
        r.type  = apvts.getRawParameterValue (params::bandId (b, "type"));
        r.freq  = apvts.getRawParameterValue (params::bandId (b, "freq"));
        r.gain  = apvts.getRawParameterValue (params::bandId (b, "gain"));
        r.q     = apvts.getRawParameterValue (params::bandId (b, "q"));
        r.focus = apvts.getRawParameterValue (params::bandId (b, "focus"));
        r.byp   = apvts.getRawParameterValue (params::bandId (b, "byp"));
    }
    apvts.state.setProperty ("presetName", "Init", nullptr);
    capturePresetSnapshot();
}

bool VeloursProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void VeloursProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate = sampleRate;
    const int tq = (pRenderUltra->load() > 0.5f && isNonRealtime()) ? 2 : juce::roundToInt (pTimeQuality->load());
    engine.prepare (sampleRate, juce::roundToInt (pQuality->load()), tq);
    setLatencySamples (engine.getLatency());
    lastWeightBins = -1;
    updateWeights (juce::jmax (1, getMainBusNumInputChannels()), pStereo->load() > 0.5f, true);
}

std::array<bands::Band, params::numBands> VeloursProcessor::readBands() const
{
    std::array<bands::Band, params::numBands> out {};
    for (int b = 0; b < params::numBands; ++b)
    {
        const auto& r = bandRaw[(size_t) b];
        auto& d = out[(size_t) b];
        d.on    = r.on->load() > 0.5f;
        d.type  = juce::roundToInt (r.type->load());
        d.freq  = r.freq->load();
        d.gain  = r.gain->load();
        d.q     = r.q->load();
        d.focus = juce::roundToInt (r.focus->load());
        d.bypass = r.byp->load() > 0.5f;
    }
    return out;
}

void VeloursProcessor::updateWeights (int numChannels, bool midSide, bool force)
{
    const auto bandsNow = readBands();
    const int bins = engine.getNumBins();
    if (! force && bandsNow == lastBands && numChannels == lastWeightChannels
        && bins == lastWeightBins && midSide == lastWeightMS)
        return;

    lastBands = bandsNow;
    lastWeightChannels = numChannels;
    lastWeightBins = bins;
    lastWeightMS = midSide;

    // Les bandes sont évaluées sur une grille logarithmique de 512 points (rapide même
    // pendant un glisser en 192 kHz), puis interpolées pour chaque case FFT.
    constexpr int gridN = 512;
    const float binHz = engine.getBinHz();
    const float fLo = 5.0f, fHi = juce::jmax (1000.0f, (float) (bins - 1) * binHz);
    const float lLo = std::log2 (fLo), lSpan = std::log2 (fHi) - lLo;
    std::array<float, gridN> g0 {}, g1 {};
    for (const auto& b : bandsNow)
    {
        if (! b.on || b.bypass) continue;
        const bool to0 = bands::appliesTo (b, 0, numChannels, midSide);
        const bool to1 = bands::appliesTo (b, 1, numChannels, midSide);
        for (int i = 0; i < gridN; ++i)
        {
            const float f = std::exp2 (lLo + lSpan * (float) i / (float) (gridN - 1));
            const float db = bands::responseDb (b, f);
            if (to0) g0[(size_t) i] += db;
            if (to1) g1[(size_t) i] += db;
        }
    }
    float* w0 = engine.getWeights (0);
    float* w1 = engine.getWeights (1);
    for (int k = 0; k < bins; ++k)
    {
        const float f = juce::jmax (fLo, (float) k * binHz);
        const float pos = juce::jlimit (0.0f, (float) (gridN - 1), (std::log2 (f) - lLo) / lSpan * (float) (gridN - 1));
        const int i0 = juce::jmin ((int) pos, gridN - 2);
        const float t = pos - (float) i0;
        w0[k] = bands::weightFromDb (g0[(size_t) i0] + t * (g0[(size_t) i0 + 1] - g0[(size_t) i0]));
        w1[k] = bands::weightFromDb (g1[(size_t) i0] + t * (g1[(size_t) i0 + 1] - g1[(size_t) i0]));
    }
}

void VeloursProcessor::updateListenMask (int band)
{
    const auto b = readBands()[(size_t) band];
    const int bins = engine.getNumBins();
    if (band == lastListenBand && b == lastListen && bins == lastListenBins) return;
    lastListenBand = band;
    lastListen = b;
    lastListenBins = bins;
    float* m = engine.getListenMask();
    const float binHz = engine.getBinHz();
    for (int k = 0; k < bins; ++k)
        m[k] = juce::jlimit (0.0f, 1.0f, bands::listenRegion (b, juce::jmax (1.0f, (float) k * binHz)));
}

velours::Settings VeloursProcessor::readSettings() const
{
    velours::Settings s;
    s.depth      = pDepth->load();
    s.detail     = pDetail->load() * 0.01f;
    s.attackMs   = pAttack->load();
    s.releaseMs  = pRelease->load();
    s.maxCutDb   = pMaxCut->load();
    s.hard       = pMode->load() > 0.5f;
    s.detailTilt = pDetailTilt->load() * 0.01f;
    s.attackTilt  = pTimeTilt->load() * 0.01f;
    s.releaseTilt = pReleaseTilt->load() * 0.01f;
    s.focus       = pFocus->load() * 0.01f;
    s.midSide    = pStereo->load() > 0.5f;
    s.link       = pLink->load() * 0.01f;
    s.sidechain  = pSidechain->load() > 0.5f;
    s.mix        = pMix->load() * 0.01f;
    s.wetTrimDb  = pWetTrim->load();
    s.outputDb   = pOutput->load();
    s.delta      = pDelta->load() > 0.5f;
    s.bypass     = pBypass->load() > 0.5f;
    s.gainMatch  = pGainMatch->load() > 0.5f;
    return s;
}

void VeloursProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int mainIn = juce::jmin (getMainBusNumInputChannels(), buffer.getNumChannels(), 2);
    const int numCh = juce::jmax (1, mainIn);
    if (n == 0 || mainIn == 0) return;

    // Résolution (change la latence ; « Zero Latency » = 0) et qualité temporelle (ne la change pas).
    // Rendu hors-ligne : qualité Ultra automatique, comme soothe3.
    const int q = juce::roundToInt (pQuality->load());
    const int tq = (pRenderUltra->load() > 0.5f && isNonRealtime()) ? 2 : juce::roundToInt (pTimeQuality->load());
    if (q != engine.getQuality() || tq != engine.getTimeQuality())
    {
        engine.setQuality (q, tq);
        setLatencySamples (engine.getLatency());
    }

    auto s = readSettings();
    updateWeights (numCh, s.midSide, false);

    // BAND LISTEN : zone de la bande en cours de réglage
    const int lb = listenBand.load();
    const auto bandsNow = readBands();
    if (juce::isPositiveAndBelow (lb, params::numBands) && bandsNow[(size_t) lb].on)
    {
        updateListenMask (lb);
        s.listen = true;
    }
    processChannels = numCh;

    // Sidechain
    auto scBus = getBusBuffer (buffer, true, 1);
    const int scCh = getBusCount (true) > 1 && getBus (true, 1)->isEnabled() ? juce::jmin (2, scBus.getNumChannels()) : 0;
    sidechainConnected = scCh > 0;
    const float* scPtr[2] = { nullptr, nullptr };
    for (int c = 0; c < scCh; ++c) scPtr[c] = scBus.getReadPointer (c);

    float* io[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (io, numCh, scCh > 0 ? scPtr : nullptr, scCh, n, s);
}

// ---------------------------------------------------------------- presets
const juce::String VeloursProcessor::getProgramName (int index)
{
    const auto& list = presets::factory();
    return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

void VeloursProcessor::loadFactoryPreset (int index)
{
    const auto& list = presets::factory();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    const auto& p = list[(size_t) index];
    currentProgram = index;
    undoManager.beginNewTransaction();

    auto set = [this] (const juce::String& id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };

    set ("depth", p.depth);       set ("detail", p.detail);
    set ("attack", p.attack);     set ("release", p.release);
    set ("maxcut", p.maxCut);     set ("mode", (float) p.mode);
    set ("detailTilt", p.detailTilt); set ("timeTilt", p.attackTilt);
    set ("releaseTilt", p.releaseTilt);
    set ("stereo", (float) p.stereo); set ("link", p.link);
    set ("mix", p.mix);           set ("wetTrim", 0.0f);
    set ("delta", 0.0f);
    set ("focus", 0.0f);

    for (int b = 0; b < params::numBands; ++b)
    {
        const auto d = params::bandDefault (b);
        bool on = false; int type = d.type; float freq = d.freq, gain = 0.0f, qv = d.q; int focus = params::focusAll;
        for (const auto& bs : p.bands)
            if (bs.slot == b) { on = true; type = bs.type; freq = bs.freq; gain = bs.gain; qv = bs.q; focus = bs.focus; }
        set (params::bandId (b, "on"), on ? 1.0f : 0.0f);
        set (params::bandId (b, "type"), (float) type);
        set (params::bandId (b, "freq"), freq);
        set (params::bandId (b, "gain"), gain);
        set (params::bandId (b, "q"), qv);
        set (params::bandId (b, "focus"), (float) focus);
        set (params::bandId (b, "byp"), 0.0f);
    }
    apvts.state.setProperty ("presetName", p.name, nullptr);
    capturePresetSnapshot();
}

// ---------------------------------------------------------------- presets utilisateur
bool VeloursProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    apvts.state.setProperty ("presetName", clean, nullptr);
    capturePresetSnapshot();
    auto state = apvts.copyState();
    for (auto* key : { "uiScale", "program", "bandListen", "version" }) state.removeProperty (key, nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (presets::userFolder().getChildFile (clean + ".velours"));
    return false;
}

void VeloursProcessor::applyStateValues (const juce::ValueTree& tree)
{
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (rp == nullptr || presets::isProtected (rp->getParameterID())) continue;
        auto child = tree.getChildWithProperty ("id", rp->getParameterID());
        // paramètre absent (preset d'une ancienne version) : valeur par défaut
        const float norm = child.isValid() ? rp->convertTo0to1 ((float) child.getProperty ("value")) : rp->getDefaultValue();
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (norm);
        rp->endChangeGesture();
    }
}

bool VeloursProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    undoManager.beginNewTransaction();
    applyStateValues (juce::ValueTree::fromXml (*xml));
    apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    capturePresetSnapshot();
    return true;
}

// ---------------------------------------------------------------- A/B
void VeloursProcessor::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot) return;
    slots[activeSlot] = apvts.copyState();
    if (! slots[slot].isValid()) slots[slot] = apvts.copyState();
    undoManager.beginNewTransaction();
    applyStateValues (slots[slot]);
    apvts.state.setProperty ("presetName", slots[slot].getProperty ("presetName", "Init"), nullptr);
    activeSlot = slot;
    capturePresetSnapshot();
}

void VeloursProcessor::copyToOtherSlot()
{
    slots[1 - activeSlot] = apvts.copyState();
}

// ---------------------------------------------------------------- état
void VeloursProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("bandListen", bandListenOnDrag.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VeloursProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            bandListenOnDrag = (bool) tree.getProperty ("bandListen", false);
            apvts.replaceState (tree);

            // Interrupteurs : valeur normalisée exacte (0 ou 1) après restauration
            for (auto* prm : getParameters())
                if (auto* bp = dynamic_cast<juce::AudioParameterBool*> (prm))
                {
                    const float snapped = bp->get() ? 1.0f : 0.0f;
                    if (! juce::exactlyEqual (static_cast<juce::AudioProcessorParameter*> (bp)->getValue(), snapped))
                        bp->setValueNotifyingHost (snapped);
                }
            undoManager.clearUndoHistory();
            capturePresetSnapshot();
        }
}

// ---------------------------------------------------------------- preset modifié ?
void VeloursProcessor::capturePresetSnapshot()
{
    presetSnapshot.clear();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            presetSnapshot.push_back (presets::isProtected (rp->getParameterID()) || rp->getParameterID() == "delta"
                                          ? -1.0f : rp->getValue());
}

bool VeloursProcessor::isPresetModified() const
{
    int i = 0;
    for (auto* prm : getParameters())
    {
        if (i >= (int) presetSnapshot.size()) return false;
        const float ref = presetSnapshot[(size_t) i++];
        if (ref >= 0.0f && std::abs (prm->getValue() - ref) > 1.0e-4f) return true;
    }
    return false;
}

juce::AudioProcessorEditor* VeloursProcessor::createEditor() { return new VeloursEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VeloursProcessor(); }

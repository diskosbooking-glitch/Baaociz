#include "PluginProcessor.h"
#include "PluginEditor.h"

VeloursProcessor::VeloursProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "VELOURS", params::createLayout())
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

    for (int b = 0; b < params::numBands; ++b)
    {
        auto& r = bandRaw[(size_t) b];
        r.on    = apvts.getRawParameterValue (params::bandId (b, "on"));
        r.type  = apvts.getRawParameterValue (params::bandId (b, "type"));
        r.freq  = apvts.getRawParameterValue (params::bandId (b, "freq"));
        r.gain  = apvts.getRawParameterValue (params::bandId (b, "gain"));
        r.q     = apvts.getRawParameterValue (params::bandId (b, "q"));
        r.focus = apvts.getRawParameterValue (params::bandId (b, "focus"));
    }
    apvts.state.setProperty ("presetName", "Init", nullptr);
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
    engine.prepare (sampleRate, juce::roundToInt (pQuality->load()));
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
        if (! b.on) continue;
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
    s.midSide    = pStereo->load() > 0.5f;
    s.link       = pLink->load() * 0.01f;
    s.sidechain  = pSidechain->load() > 0.5f;
    s.mix        = pMix->load() * 0.01f;
    s.wetTrimDb  = pWetTrim->load();
    s.outputDb   = pOutput->load();
    s.delta      = pDelta->load() > 0.5f;
    s.bypass     = pBypass->load() > 0.5f;
    return s;
}

void VeloursProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int mainIn = juce::jmin (getMainBusNumInputChannels(), buffer.getNumChannels(), 2);
    const int numCh = juce::jmax (1, mainIn);
    if (n == 0 || mainIn == 0) return;

    // Résolution (change la latence)
    const int q = juce::roundToInt (pQuality->load());
    if (q != engine.getQuality())
    {
        engine.setQuality (q);
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
    }
    apvts.state.setProperty ("presetName", p.name, nullptr);
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
        }
}

juce::AudioProcessorEditor* VeloursProcessor::createEditor() { return new VeloursEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VeloursProcessor(); }

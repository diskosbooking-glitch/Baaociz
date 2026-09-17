#include "PluginProcessor.h"
#include "PluginEditor.h"

SubShaperProcessor::SubShaperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", params::createLayout())
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            raw[rp->getParameterID().toStdString()] = apvts.getRawParameterValue (rp->getParameterID());

    apvts.state.setProperty ("presetName", "Init", nullptr);
}

float SubShaperProcessor::getKeyFrequency() const
{
    return params::noteFrequency ((int) p ("keyNote"), (int) p ("keyOct"));
}

bool SubShaperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SubShaperProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseRate = sampleRate;
    currentSampleRate = sampleRate;
    preparedBlockSize = juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) preparedBlockSize, 2 };

    crossover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover.prepare (spec);
    crossover.setCutoffFrequency (p ("crossover"));

    subCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    subCut.setCutoffFrequency (25.0f);
    subCut.setResonance (0.707f);
    subCut.prepare (spec);

    using OS = juce::dsp::Oversampling<float>;
    osHQ  = std::make_unique<OS> (2, 2, OS::filterHalfBandFIREquiripple, true, true);
    osEco = std::make_unique<OS> (2, 1, OS::filterHalfBandFIREquiripple, true, true);
    osHQ->initProcessing ((size_t) preparedBlockSize);
    osEco->initProcessing ((size_t) preparedBlockSize);

    dryLowDelay.prepare (spec);
    highDelay.prepare (spec);
    dryLowDelay.setMaximumDelayInSamples (256);
    highDelay.setMaximumDelayInSamples (256);

    for (auto& s : shapers) s.prepare (sampleRate);

    for (auto* s : { &inGainSm, &crossoverSm, &mixSm, &outputSm, &attackSm, &sustainSm,
                     &shapeEnSm, &pumpEnSm, &pumpDepthSm, &widthSm, &widthEnSm })
        s->reset (sampleRate, 0.03);

    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("inGain")));
    crossoverSm.setCurrentAndTargetValue (p ("crossover"));
    mixSm.setCurrentAndTargetValue (p ("mix") * 0.01f);
    outputSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("output")));
    attackSm.setCurrentAndTargetValue (p ("attack") * 0.01f);
    sustainSm.setCurrentAndTargetValue (p ("sustain") * 0.01f);
    shapeEnSm.setCurrentAndTargetValue (b ("shapeOn") ? 1.0f : 0.0f);
    pumpEnSm.setCurrentAndTargetValue (b ("pumpOn") ? 1.0f : 0.0f);
    pumpDepthSm.setCurrentAndTargetValue (p ("pumpDepth") * 0.01f);
    widthSm.setCurrentAndTargetValue (p ("width") * 0.01f);
    widthEnSm.setCurrentAndTargetValue (b ("widthOn") ? 1.0f : 0.0f);

    msIn = msOut = 0.0f;
    matchGain = 1.0f;
    freePhaseBeats = 0.0;
    lastGenType = -1;

    ensureBuffers (preparedBlockSize);
    oversampler = nullptr;
    configureQuality (b ("hq"));
}

void SubShaperProcessor::configureQuality (bool hq)
{
    activeHQ = hq;
    oversampler = hq ? osHQ.get() : osEco.get();
    oversampler->reset();
    latency = (int) std::lround (oversampler->getLatencyInSamples());
    setLatencySamples (latency);
    dryLowDelay.setDelay ((float) latency);
    highDelay.setDelay ((float) latency);

    const double osRate = baseRate * (double) oversampler->getOversamplingFactor();
    for (auto& e : engines) e.prepare (osRate);

    for (auto* s : { &keyFreqSm, &genEnSm, &genLevelSm, &genToneSm, &toneEnSm, &toneAmtSm, &toneQSm,
                     &driveEnSm, &driveSm, &colorSm, &focusSm, &driveMixSm })
        s->reset (osRate, 0.03);

    keyFreqSm.setCurrentAndTargetValue (getKeyFrequency());
    genEnSm.setCurrentAndTargetValue (b ("genOn") ? 1.0f : 0.0f);
    genLevelSm.setCurrentAndTargetValue (p ("genLevel") * 0.01f);
    genToneSm.setCurrentAndTargetValue (p ("genTone") * 0.01f);
    toneEnSm.setCurrentAndTargetValue (b ("toneOn") ? 1.0f : 0.0f);
    toneAmtSm.setCurrentAndTargetValue (p ("toneAmt") * 0.01f);
    toneQSm.setCurrentAndTargetValue (p ("toneQ") * 0.01f);
    driveEnSm.setCurrentAndTargetValue (b ("driveOn") ? 1.0f : 0.0f);
    driveSm.setCurrentAndTargetValue (p ("drive") * 0.01f);
    colorSm.setCurrentAndTargetValue (p ("color") * 0.01f);
    focusSm.setCurrentAndTargetValue (p ("focus") * 0.01f);
    driveMixSm.setCurrentAndTargetValue (p ("driveMix") * 0.01f);
}

void SubShaperProcessor::ensureBuffers (int numSamples)
{
    if (lowBuffer.getNumSamples() < numSamples)
    {
        lowBuffer.setSize (2, numSamples, false, false, true);
        dryLowBuffer.setSize (2, numSamples, false, false, true);
        highBuffer.setSize (2, numSamples, false, false, true);
    }
}

void SubShaperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (n == 0 || numCh == 0 || oversampler == nullptr)
        return;

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    if (n > preparedBlockSize)
    {
        preparedBlockSize = n;
        osHQ->initProcessing ((size_t) n);
        osEco->initProcessing ((size_t) n);
        ensureBuffers (n);
    }

    if (b ("hq") != activeHQ)
        configureQuality (b ("hq"));

    // --- Tempo hôte ---
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto v = pos->getBpm()) bpm = *v;
            if (auto v = pos->getPpqPosition()) ppq = *v;
            playing = pos->getIsPlaying();
        }
    hostBpm = bpm;

    // --- Cibles des lissages ---
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (p ("inGain")));
    crossoverSm.setTargetValue (p ("crossover"));
    mixSm.setTargetValue (p ("mix") * 0.01f);
    outputSm.setTargetValue (juce::Decibels::decibelsToGain (p ("output")));
    attackSm.setTargetValue (p ("attack") * 0.01f);
    sustainSm.setTargetValue (p ("sustain") * 0.01f);
    shapeEnSm.setTargetValue (b ("shapeOn") ? 1.0f : 0.0f);
    pumpEnSm.setTargetValue (b ("pumpOn") ? 1.0f : 0.0f);
    pumpDepthSm.setTargetValue (p ("pumpDepth") * 0.01f);
    widthSm.setTargetValue (p ("width") * 0.01f);
    widthEnSm.setTargetValue (b ("widthOn") ? 1.0f : 0.0f);

    keyFreqSm.setTargetValue (getKeyFrequency());
    genEnSm.setTargetValue (b ("genOn") ? 1.0f : 0.0f);
    genLevelSm.setTargetValue (p ("genLevel") * 0.01f);
    genToneSm.setTargetValue (p ("genTone") * 0.01f);
    toneEnSm.setTargetValue (b ("toneOn") ? 1.0f : 0.0f);
    toneAmtSm.setTargetValue (p ("toneAmt") * 0.01f);
    toneQSm.setTargetValue (p ("toneQ") * 0.01f);
    driveEnSm.setTargetValue (b ("driveOn") ? 1.0f : 0.0f);
    driveSm.setTargetValue (p ("drive") * 0.01f);
    colorSm.setTargetValue (p ("color") * 0.01f);
    focusSm.setTargetValue (p ("focus") * 0.01f);
    driveMixSm.setTargetValue (p ("driveMix") * 0.01f);

    const int genType = (int) p ("genType");
    if (genType != lastGenType)
    {
        for (auto& e : engines) e.reset();
        lastGenType = genType;
    }

    const bool monoLow = b ("monoLow"), soloLow = b ("soloLow"), cutSub = b ("subCut");
    const bool delta = b ("delta"), gainMatch = b ("gainMatch");

    // 1) Gain d'entrée + séparation grave / aigu
    for (int i = 0; i < n; ++i)
    {
        if (crossoverSm.isSmoothing() || i == 0)
            crossover.setCutoffFrequency (crossoverSm.getNextValue());
        const float g = inGainSm.getNextValue();

        float inMono = 0.0f;
        float lo[2] {}, hi[2] {};
        for (int ch = 0; ch < 2; ++ch)
        {
            const float x = buffer.getSample (juce::jmin (ch, numCh - 1), i);
            inMono += 0.5f * x;
            crossover.processSample (ch, x * g, lo[ch], hi[ch]);
        }
        msIn += 0.0005f * (inMono * inMono - msIn);

        if (monoLow || numCh == 1)
            lo[0] = lo[1] = 0.5f * (lo[0] + lo[1]);

        // Accordeur : grave sec, décimé x4
        tunerAccum += 0.5f * (lo[0] + lo[1]);
        if (++tunerCount == 4)
        {
            const auto w = tunerFifo.write (1);
            if (w.blockSize1 > 0) tunerData[(size_t) w.startIndex1] = tunerAccum * 0.25f;
            tunerAccum = 0.0f;
            tunerCount = 0;
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            lowBuffer.setSample (ch, i, lo[ch]);
            dryLowBuffer.setSample (ch, i, lo[ch]);
            highBuffer.setSample (ch, i, hi[ch]);
        }
    }

    // 2) Chaîne GEN -> TONE -> DRIVE en suréchantillonné
    juce::dsp::AudioBlock<float> lowBlock (lowBuffer.getArrayOfWritePointers(), 2, (size_t) n);
    auto osBlock = oversampler->processSamplesUp (lowBlock);
    auto* o0 = osBlock.getChannelPointer (0);
    auto* o1 = osBlock.getChannelPointer (1);

    subshaper::EngineParams ep;
    ep.genType   = genType;
    ep.keyLock   = b ("keyLock");
    ep.toneHarm  = (int) p ("toneHarm");
    ep.driveType = (int) p ("driveType");

    for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
    {
        ep.keyFreq  = keyFreqSm.getNextValue();
        ep.genEn    = genEnSm.getNextValue();
        ep.genLevel = genLevelSm.getNextValue();
        ep.genTone  = genToneSm.getNextValue();
        ep.toneEn   = toneEnSm.getNextValue();
        ep.toneAmt  = toneAmtSm.getNextValue();
        ep.toneQ    = toneQSm.getNextValue();
        ep.driveEn  = driveEnSm.getNextValue();
        ep.drive    = driveSm.getNextValue();
        ep.color    = colorSm.getNextValue();
        ep.focus    = focusSm.getNextValue();
        ep.driveMix = driveMixSm.getNextValue();
        o0[i] = engines[0].process (o0[i], ep);
        o1[i] = engines[1].process (o1[i], ep);
    }
    oversampler->processSamplesDown (lowBlock);

    // 3) SHAPE, PUMP, recombinaison, WIDTH, sorties
    static const double rateBeats[] { 4.0, 2.0, 1.0, 0.5, 0.25 };
    const double cycle = rateBeats[juce::jlimit (0, 4, (int) p ("pumpRate"))];
    const double beatsPerSample = bpm / 60.0 / baseRate;
    const float release = 0.15f + p ("pumpShape") * 0.01f * 0.75f;
    double beatPos = playing ? ppq : freePhaseBeats;

    float peakL = meterL.load(), peakR = meterR.load();
    float lastPumpGain = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        const float mix = mixSm.getNextValue();
        const float outGain = outputSm.getNextValue();
        const float att = attackSm.getNextValue(), sus = sustainSm.getNextValue();
        const float shapeEn = shapeEnSm.getNextValue();
        const float pumpEn = pumpEnSm.getNextValue();
        const float depth = pumpDepthSm.getNextValue();
        const float width = widthSm.getNextValue();
        const float widthEn = widthEnSm.getNextValue();

        // Enveloppe de pompe
        double ph = std::fmod (beatPos / cycle, 1.0);
        if (ph < 0.0) ph += 1.0;
        const float t = juce::jmin (1.0f, (float) ph / release);
        const float curve = std::sin (t * juce::MathConstants<float>::halfPi);
        const float pumpGain = 1.0f - pumpEn * depth * (1.0f - curve * curve);
        beatPos += beatsPerSample;
        lastPumpGain = pumpGain;

        float dry[2], hi[2], hiRef[2], lowOut[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            dry[ch] = dryLowDelay.popSample (ch);
            dryLowDelay.pushSample (ch, dryLowBuffer.getSample (ch, i));
            hi[ch] = highDelay.popSample (ch);
            highDelay.pushSample (ch, highBuffer.getSample (ch, i));
            hiRef[ch] = hi[ch];

            float wet = lowBuffer.getSample (ch, i);
            if (shapeEn > 0.0f)
                wet += shapeEn * (shapers[(size_t) ch].process (wet, att, sus) - wet);
            wet *= pumpGain;
            lowOut[ch] = dry[ch] + mix * (wet - dry[ch]);
        }

        if (numCh == 2 && widthEn > 0.0f)
        {
            const float m = 0.5f * (hi[0] + hi[1]);
            const float s = 0.5f * (hi[0] - hi[1]) * (1.0f + widthEn * (width - 1.0f));
            hi[0] = m + s;
            hi[1] = m - s;
        }

        float outMono = 0.0f;
        float out[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            float y = soloLow ? lowOut[ch] : lowOut[ch] + hi[ch];
            if (delta)
                y -= soloLow ? dry[ch] : dry[ch] + hiRef[ch];
            if (cutSub)
                y = subCut.processSample (ch, y);
            out[ch] = y;
            outMono += 0.5f * y;
        }

        msOut += 0.0005f * (outMono * outMono - msOut);
        if (gainMatch && ! delta && msIn > 1.0e-8f && msOut > 1.0e-8f)
        {
            const float target = juce::jlimit (0.25f, 4.0f, std::sqrt (msIn / msOut) * matchGain);
            matchGain += 0.0002f * (target - matchGain);
        }
        else
        {
            matchGain += 0.001f * (1.0f - matchGain);
        }

        const float finalGain = outGain * (gainMatch && ! delta ? matchGain : 1.0f);
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, out[ch] * finalGain);

        peakL = juce::jmax (peakL, std::abs (out[0] * finalGain));
        peakR = juce::jmax (peakR, std::abs (out[numCh > 1 ? 1 : 0] * finalGain));
    }
    freePhaseBeats = std::fmod (beatPos, 16.0);
    pumpGainNow = lastPumpGain;
    meterL = peakL;
    meterR = peakR;

    // 4) Analyseur
    const auto scope = analyserFifo.write (n);
    auto push = [&] (int start, int size, int offset)
    {
        for (int k = 0; k < size; ++k)
        {
            float s = buffer.getSample (0, offset + k);
            if (numCh > 1) s = 0.5f * (s + buffer.getSample (1, offset + k));
            analyserData[(size_t) (start + k)] = s;
        }
    };
    push (scope.startIndex1, scope.blockSize1, 0);
    push (scope.startIndex2, scope.blockSize2, scope.blockSize1);
}

// ============================================================================
//  Presets, A/B, état
// ============================================================================
void SubShaperProcessor::setCurrentProgram (int index)
{
    // Certains hôtes rappellent le programme courant au chargement : on ne réécrase pas l'état.
    if (index != currentProgram)
        loadFactoryPreset (index);
}

void SubShaperProcessor::loadFactoryPreset (int index)
{
    const auto& list = presets::factory();
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;
    currentProgram = index;
    presets::apply (apvts, list[(size_t) index]);
    apvts.state.setProperty ("presetName", juce::String (list[(size_t) index].name), nullptr);
}

const juce::String SubShaperProcessor::getProgramName (int index)
{
    const auto& list = presets::factory();
    return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

bool SubShaperProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;
    apvts.state.setProperty ("presetName", clean, nullptr);
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        return xml->writeTo (presets::userFolder().getChildFile (clean + ".ssp"));
    return false;
}

bool SubShaperProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (rp == nullptr || presets::isProtected (rp->getParameterID()))
            continue;
        auto child = tree.getChildWithProperty ("id", rp->getParameterID());
        if (! child.isValid())
            continue;
        const float v = (float) child.getProperty ("value");
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        rp->endChangeGesture();
    }
    apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    return true;
}

void SubShaperProcessor::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot)
        return;
    slots[activeSlot] = apvts.copyState();
    if (! slots[slot].isValid())
        slots[slot] = apvts.copyState();
    const auto scale = apvts.state.getProperty ("uiScale", 1.0);
    apvts.replaceState (slots[slot].createCopy());
    apvts.state.setProperty ("uiScale", scale, nullptr);
    activeSlot = slot;
    undoManager.clearUndoHistory();
}

void SubShaperProcessor::copyToOtherSlot()
{
    slots[1 - activeSlot] = apvts.copyState();
}

void SubShaperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SubShaperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SubShaperProcessor::createEditor()
{
    return new SubShaperEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubShaperProcessor();
}

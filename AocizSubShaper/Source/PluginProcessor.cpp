#include "PluginProcessor.h"
#include "PluginEditor.h"

SubShaperProcessor::SubShaperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "PARAMS", params::createLayout())
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            raw[rp->getParameterID().toStdString()] = apvts.getRawParameterValue (rp->getParameterID());

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
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
    if (layouts.getMainInputChannelSet() != out)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void SubShaperProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseRate = sampleRate;
    currentSampleRate = sampleRate;
    preparedBlockSize = juce::jmax (1, samplesPerBlock);

    using OS = juce::dsp::Oversampling<float>;
    osHQ  = std::make_unique<OS> (2, 2, OS::filterHalfBandFIREquiripple, true, true);
    osEco = std::make_unique<OS> (2, 1, OS::filterHalfBandFIREquiripple, true, true);
    osHQ->initProcessing ((size_t) preparedBlockSize);
    osEco->initProcessing ((size_t) preparedBlockSize);

    ensureBuffers (preparedBlockSize);
    bypassRing.setSize (2, 8192);
    bypassRing.clear();
    bypassWrite = 0;

    inGainSm.reset (sampleRate, 0.03);
    bypassSm.reset (sampleRate, 0.03);
    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("inGain")));
    bypassSm.setCurrentAndTargetValue (b ("bypass") ? 1.0f : 0.0f);

    kick.prepare (sampleRate);

    msIn = msOut = 0.0f;
    matchGain = 1.0f;
    freePhaseBeats = 0.0;
    lastGenType = -1;

    oversampler = nullptr;
    configureQuality (b ("hq"));
}

void SubShaperProcessor::configureQuality (bool hq)
{
    activeHQ = hq;
    oversampler = hq ? osHQ.get() : osEco.get();
    oversampler->reset();
    factor = (int) oversampler->getOversamplingFactor();
    latency = (int) std::lround (oversampler->getLatencyInSamples());
    setLatencySamples (latency);

    osRate = baseRate * (double) factor;
    juce::dsp::ProcessSpec osSpec { osRate, (juce::uint32) (preparedBlockSize * factor), 2 };

    crossover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover.prepare (osSpec);
    crossover.setCutoffFrequency (p ("crossover"));

    subCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    subCut.setCutoffFrequency (25.0f);
    subCut.setResonance (0.707f);
    subCut.prepare (osSpec);

    for (auto& e : engines) e.prepare (osRate);
    for (auto& d : dynamics) d.prepare (osRate);

    for (auto* s : { &crossoverSm, &mixSm, &outputSm, &attackSm, &sustainSm, &squashSm, &shapeEnSm,
                     &pumpEnSm, &pumpDepthSm, &widthSm, &widthEnSm, &clipEnSm, &clipPushSm, &clipCeilSm,
                     &keyFreqSm, &genEnSm, &genLevelSm, &genToneSm, &toneEnSm, &toneAmtSm, &toneQSm,
                     &driveEnSm, &driveSm, &colorSm, &focusSm, &driveMixSm })
        s->reset (osRate, 0.03);

    crossoverSm.setCurrentAndTargetValue (p ("crossover"));
    mixSm.setCurrentAndTargetValue (p ("mix") * 0.01f);
    outputSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("output")));
    attackSm.setCurrentAndTargetValue (p ("attack") * 0.01f);
    sustainSm.setCurrentAndTargetValue (p ("sustain") * 0.01f);
    squashSm.setCurrentAndTargetValue (p ("squash") * 0.01f);
    shapeEnSm.setCurrentAndTargetValue (b ("shapeOn") ? 1.0f : 0.0f);
    pumpEnSm.setCurrentAndTargetValue (b ("pumpOn") ? 1.0f : 0.0f);
    pumpDepthSm.setCurrentAndTargetValue (p ("pumpDepth") * 0.01f);
    widthSm.setCurrentAndTargetValue (p ("width") * 0.01f);
    widthEnSm.setCurrentAndTargetValue (b ("widthOn") ? 1.0f : 0.0f);
    clipEnSm.setCurrentAndTargetValue (b ("clipOn") ? 1.0f : 0.0f);
    clipPushSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("clipPush")));
    clipCeilSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("clipCeil")));

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

    tunerAccum = 0.0f;
    tunerCount = 0;
}

void SubShaperProcessor::ensureBuffers (int numSamples)
{
    if (work.getNumSamples() < numSamples)
    {
        work.setSize (2, numSamples, false, false, true);
        dryDelayed.setSize (2, numSamples, false, false, true);
    }
    if ((int) kickEnv.size() < numSamples)
        kickEnv.resize ((size_t) numSamples, 0.0f);
}

void SubShaperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    auto mainBus = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (2, mainBus.getNumChannels(), getTotalNumOutputChannels());
    if (n == 0 || numCh == 0 || oversampler == nullptr)
        return;

    if (n > preparedBlockSize)
    {
        preparedBlockSize = n;
        osHQ->initProcessing ((size_t) n);
        osEco->initProcessing ((size_t) n);
        ensureBuffers (n);
    }

    if (b ("hq") != activeHQ)
        configureQuality (b ("hq"));

    // --- Entrée sidechain (kick) ---
    const float* sc[2] { nullptr, nullptr };
    int scNum = 0;
    if (getBusCount (true) > 1)
        if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled())
        {
            auto scBuf = getBusBuffer (buffer, true, 1);
            scNum = juce::jmin (2, scBuf.getNumChannels());
            for (int ch = 0; ch < scNum; ++ch)
                sc[ch] = scBuf.getReadPointer (ch);
        }

    // --- Tempo hôte ---
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, hasTempo = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto v = pos->getBpm()) { bpm = *v; hasTempo = true; }
            if (auto v = pos->getPpqPosition()) ppq = *v;
            playing = pos->getIsPlaying();
        }
    if (bpm < 20.0 || bpm > 999.0) bpm = 120.0;
    hostBpm = bpm;
    hostHasTempo = hasTempo;

    // --- Cibles des lissages ---
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (p ("inGain")));
    bypassSm.setTargetValue (b ("bypass") ? 1.0f : 0.0f);
    crossoverSm.setTargetValue (p ("crossover"));
    mixSm.setTargetValue (p ("mix") * 0.01f);
    outputSm.setTargetValue (juce::Decibels::decibelsToGain (p ("output")));
    attackSm.setTargetValue (p ("attack") * 0.01f);
    sustainSm.setTargetValue (p ("sustain") * 0.01f);
    squashSm.setTargetValue (p ("squash") * 0.01f);
    shapeEnSm.setTargetValue (b ("shapeOn") ? 1.0f : 0.0f);
    pumpEnSm.setTargetValue (b ("pumpOn") ? 1.0f : 0.0f);
    pumpDepthSm.setTargetValue (p ("pumpDepth") * 0.01f);
    widthSm.setTargetValue (p ("width") * 0.01f);
    widthEnSm.setTargetValue (b ("widthOn") ? 1.0f : 0.0f);
    clipEnSm.setTargetValue (b ("clipOn") ? 1.0f : 0.0f);
    clipPushSm.setTargetValue (juce::Decibels::decibelsToGain (p ("clipPush")));
    clipCeilSm.setTargetValue (juce::Decibels::decibelsToGain (p ("clipCeil")));

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
    const bool pumpKick = p ("pumpSrc") > 0.5f;
    const bool clipOn = b ("clipOn"), clipHard = p ("clipType") > 0.5f;

    // Retour du ducking en mode KICK : 30 ms -> 600 ms
    kick.setRelease (30.0f * std::pow (20.0f, p ("pumpShape") * 0.01f));

    // =====================================================================
    // 1) Fréquence de base : entrée, bypass retardé, gain d'entrée, sidechain
    // =====================================================================
    const int ringSize = bypassRing.getNumSamples();
    float kickMax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.getNextValue();
        float x[2];
        x[0] = mainBus.getSample (0, i);
        x[1] = numCh > 1 ? mainBus.getSample (1, i) : x[0];

        const int readPos = (bypassWrite - latency + ringSize) % ringSize;
        for (int ch = 0; ch < 2; ++ch)
        {
            bypassRing.setSample (ch, bypassWrite, x[ch]);
            dryDelayed.setSample (ch, i, bypassRing.getSample (ch, readPos));
            work.setSample (ch, i, x[ch] * g);
        }
        bypassWrite = (bypassWrite + 1) % ringSize;

        const float m = 0.5f * (x[0] + x[1]);
        msIn += 0.0005f * (m * m - msIn);

        float s = 0.0f;
        if (scNum > 0)
            s = scNum > 1 ? 0.5f * (sc[0][i] + sc[1][i]) : sc[0][i];
        const float k = kick.process (s);
        kickEnv[(size_t) i] = k;
        kickMax = juce::jmax (kickMax, k);
    }
    sidechainActive = scNum > 0 && kick.peak > 0.003f;
    kickLevel = kickMax;

    // =====================================================================
    // 2) Suréchantillonné : séparation, modules, recombinaison, CLIP
    // =====================================================================
    juce::dsp::AudioBlock<float> baseBlock (work.getArrayOfWritePointers(), 2, (size_t) n);
    auto osBlock = oversampler->processSamplesUp (baseBlock);
    auto* o0 = osBlock.getChannelPointer (0);
    auto* o1 = osBlock.getChannelPointer (1);
    const int N = (int) osBlock.getNumSamples();

    subshaper::EngineParams ep;
    ep.genType   = genType;
    ep.keyLock   = b ("keyLock");
    ep.toneHarm  = (int) p ("toneHarm");
    ep.toneTrack = b ("toneTrack");
    ep.driveType = (int) p ("driveType");
    ep.driveHarm = b ("driveHarm");

    static const double rateBeats[] { 4.0, 2.0, 1.0, 0.5, 0.25 };
    const double cycle = rateBeats[juce::jlimit (0, 4, (int) p ("pumpRate"))];
    const double beatsPerSample = bpm / 60.0 / osRate;
    const float syncRelease = 0.15f + p ("pumpShape") * 0.01f * 0.75f;
    double beatPos = playing ? ppq : freePhaseBeats;

    const float fac = (float) factor;
    const float msCoef = 0.0005f / fac, matchUp = 0.0002f / fac, matchRelax = 0.001f / fac;
    const int tunerDecim = factor * 4;

    float lastPumpGain = 1.0f, clipMinRatio = 1.0f;

    for (int j = 0; j < N; ++j)
    {
        if (crossoverSm.isSmoothing() || j == 0)
            crossover.setCutoffFrequency (crossoverSm.getNextValue());

        const float mix = mixSm.getNextValue();
        const float outGain = outputSm.getNextValue();
        const float att = attackSm.getNextValue(), sus = sustainSm.getNextValue(), squash = squashSm.getNextValue();
        const float shapeEn = shapeEnSm.getNextValue();
        const float pumpEn = pumpEnSm.getNextValue();
        const float depth = pumpDepthSm.getNextValue();
        const float width = widthSm.getNextValue();
        const float widthEn = widthEnSm.getNextValue();
        const float clipEn = clipEnSm.getNextValue();
        const float push = clipPushSm.getNextValue();
        const float ceiling = clipCeilSm.getNextValue();

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

        // Séparation grave / aigu
        float lo[2], hi[2];
        crossover.processSample (0, o0[j], lo[0], hi[0]);
        crossover.processSample (1, o1[j], lo[1], hi[1]);
        if (monoLow || numCh == 1)
            lo[0] = lo[1] = 0.5f * (lo[0] + lo[1]);

        // Accordeur : grave sec, ramené à fs/4
        tunerAccum += 0.5f * (lo[0] + lo[1]);
        if (++tunerCount == tunerDecim)
        {
            const auto w = tunerFifo.write (1);
            if (w.blockSize1 > 0) tunerData[(size_t) w.startIndex1] = tunerAccum / (float) tunerDecim;
            tunerAccum = 0.0f;
            tunerCount = 0;
        }

        const float dryLo[2] { lo[0], lo[1] };

        // GEN -> TONE -> DRIVE
        lo[0] = engines[0].process (lo[0], ep);
        lo[1] = engines[1].process (lo[1], ep);

        // SHAPE : squash + transitoires
        if (shapeEn > 0.0f)
            for (int ch = 0; ch < 2; ++ch)
                lo[ch] += shapeEn * (dynamics[(size_t) ch].process (lo[ch], att, sus, squash) - lo[ch]);

        // PUMP : synchro tempo ou kick (sidechain)
        float duck;
        if (pumpKick)
        {
            duck = kickEnv[(size_t) juce::jmin (j / factor, n - 1)];
        }
        else
        {
            double ph = std::fmod (beatPos / cycle, 1.0);
            if (ph < 0.0) ph += 1.0;
            const float t = juce::jmin (1.0f, (float) ph / syncRelease);
            const float c = std::sin (t * juce::MathConstants<float>::halfPi);
            duck = 1.0f - c * c;
        }
        beatPos += beatsPerSample;
        const float pumpGain = 1.0f - pumpEn * depth * duck;
        lastPumpGain = pumpGain;

        // WIDTH au-dessus de la coupure
        float hiW[2] { hi[0], hi[1] };
        if (numCh == 2 && widthEn > 0.0f)
        {
            const float mm = 0.5f * (hi[0] + hi[1]);
            const float ss = 0.5f * (hi[0] - hi[1]) * (1.0f + widthEn * (width - 1.0f));
            hiW[0] = mm + ss;
            hiW[1] = mm - ss;
        }

        // Recombinaison
        float y[2], ref[2];
        float wetMono = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float lowOut = dryLo[ch] + mix * (lo[ch] * pumpGain - dryLo[ch]);
            float w = soloLow ? lowOut : lowOut + hiW[ch];
            ref[ch] = soloLow ? dryLo[ch] : dryLo[ch] + hi[ch];
            if (cutSub)
                w = subCut.processSample (ch, w);
            y[ch] = w;
            wetMono += 0.5f * w;
        }

        // Égalisation de volume (mesure avant OUTPUT)
        msOut += msCoef * (wetMono * wetMono - msOut);
        if (gainMatch && ! delta && msIn > 1.0e-8f && msOut > 1.0e-8f)
            matchGain += matchUp * (juce::jlimit (0.25f, 4.0f, std::sqrt (msIn / msOut)) - matchGain);
        else
            matchGain += matchRelax * (1.0f - matchGain);

        const float g = outGain * (gainMatch && ! delta ? matchGain : 1.0f);

        for (int ch = 0; ch < 2; ++ch)
        {
            float v = y[ch] * g;
            if (clipEn > 0.0f)
            {
                const float in = v * push;
                const float c = subshaper::clipSample (in, ceiling, clipHard);
                if (std::abs (in) > 1.0e-4f)
                    clipMinRatio = juce::jmin (clipMinRatio, std::abs (c) / std::abs (in));
                v += clipEn * (c - v);
            }
            if (delta)
                v -= ref[ch] * outGain;
            (ch == 0 ? o0 : o1)[j] = v;
        }
    }
    freePhaseBeats = std::fmod (beatPos, 16.0);
    pumpGainNow = lastPumpGain;

    oversampler->processSamplesDown (baseBlock);

    // =====================================================================
    // 3) Fréquence de base : plafond garanti, bypass, vumètres
    // =====================================================================
    const bool safety = clipOn && ! delta;
    const float ceilLin = juce::Decibels::decibelsToGain (p ("clipCeil"));
    float peakL = meterL.load(), peakR = meterR.load();
    float blockPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float bp = bypassSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float v = work.getSample (ch, i);
            if (safety)
                v = juce::jlimit (-ceilLin, ceilLin, v);
            v += bp * (dryDelayed.getSample (ch, i) - v);
            buffer.setSample (ch, i, v);
        }
        const float l = std::abs (buffer.getSample (0, i));
        const float r = std::abs (buffer.getSample (numCh > 1 ? 1 : 0, i));
        peakL = juce::jmax (peakL, l);
        peakR = juce::jmax (peakR, r);
        blockPeak = juce::jmax (blockPeak, l, r);
    }
    meterL = peakL;
    meterR = peakR;
    if (blockPeak > outPeak.load())
        outPeak = blockPeak;

    const float reduction = -juce::Decibels::gainToDecibels (clipMinRatio, -60.0f);
    if (clipOn && reduction > clipReduction.load())
        clipReduction = reduction;

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
        // Paramètre absent (preset d'une ancienne version) : valeur par défaut
        const float norm = child.isValid() ? rp->convertTo0to1 ((float) child.getProperty ("value"))
                                           : rp->getDefaultValue();
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (norm);
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
    const float bypassNow = bypassParam != nullptr && bypassParam->get() ? 1.0f : 0.0f;
    apvts.replaceState (slots[slot].createCopy());
    apvts.state.setProperty ("uiScale", scale, nullptr);
    if (bypassParam != nullptr)
        bypassParam->setValueNotifyingHost (bypassNow);
    activeSlot = slot;
    undoManager.clearUndoHistory();
}

void SubShaperProcessor::copyToOtherSlot()
{
    slots[1 - activeSlot] = apvts.copyState();
}

void SubShaperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SubShaperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            // Interrupteurs : valeur normalisée exacte (0 ou 1) après restauration
            for (auto* prm : getParameters())
                if (auto* bp = dynamic_cast<juce::AudioParameterBool*> (prm))
                {
                    const float snapped = bp->get() ? 1.0f : 0.0f;
                    if (static_cast<juce::AudioProcessorParameter*> (bp)->getValue() != snapped)
                        bp->setValueNotifyingHost (snapped);
                }
        }
}

juce::AudioProcessorEditor* SubShaperProcessor::createEditor()
{
    return new SubShaperEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubShaperProcessor();
}

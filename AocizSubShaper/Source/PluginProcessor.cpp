#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

SubShaperProcessor::SubShaperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pCrossover = apvts.getRawParameterValue ("crossover");
    pAmount    = apvts.getRawParameterValue ("amount");
    pCharacter = apvts.getRawParameterValue ("character");
    pMix       = apvts.getRawParameterValue ("mix");
    pOutput    = apvts.getRawParameterValue ("output");
    pMode      = apvts.getRawParameterValue ("mode");
    pMonoLow   = apvts.getRawParameterValue ("monoLow");
    pSoloLow   = apvts.getRawParameterValue ("soloLow");
    pSubCut    = apvts.getRawParameterValue ("subCut");
}

APVTS::ParameterLayout SubShaperProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    NormalisableRange<float> xoverRange (40.0f, 400.0f, 0.1f);
    xoverRange.setSkewForCentre (120.0f);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                     StringArray { "Saturate", "Resonate", "Octave", "Synthesize", "Fold" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "crossover", 1 }, "Crossover", xoverRange, 120.0f,
                     AudioParameterFloatAttributes().withLabel ("Hz")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
                     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f,
                     AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "character", 1 }, "Character",
                     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
                     AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                     AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                     NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                     AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "monoLow", 1 }, "Mono Low", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "soloLow", 1 }, "Solo Low", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "subCut", 1 }, "Sub Cut", true));

    return { p.begin(), p.end() };
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
    currentSampleRate = sampleRate;
    preparedBlockSize = juce::jmax (1, samplesPerBlock);
    const auto numCh = (juce::uint32) 2;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) preparedBlockSize, numCh };

    crossover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover.prepare (spec);
    crossover.setCutoffFrequency (pCrossover->load());

    subCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    subCut.setCutoffFrequency (25.0f);
    subCut.setResonance (0.707f);
    subCut.prepare (spec);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        numCh, osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) preparedBlockSize);
    latency = (int) std::lround (oversampler->getLatencyInSamples());
    setLatencySamples (latency);

    dryLowDelay.prepare (spec);
    highDelay.prepare (spec);
    dryLowDelay.setMaximumDelayInSamples (juce::jmax (8, latency + 8));
    highDelay.setMaximumDelayInSamples (juce::jmax (8, latency + 8));
    dryLowDelay.setDelay ((float) latency);
    highDelay.setDelay ((float) latency);

    const double osRate = sampleRate * (1 << osFactorLog2);
    for (auto& e : engines)
        e.prepare (osRate);

    crossoverSm.reset (sampleRate, 0.05);
    mixSm.reset (sampleRate, 0.05);
    outputSm.reset (sampleRate, 0.05);
    amountSm.reset (osRate, 0.05);
    characterSm.reset (osRate, 0.05);

    crossoverSm.setCurrentAndTargetValue (pCrossover->load());
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    outputSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    amountSm.setCurrentAndTargetValue (pAmount->load() * 0.01f);
    characterSm.setCurrentAndTargetValue (pCharacter->load() * 0.01f);

    ensureBuffers (preparedBlockSize);
    lastMode = -1;
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

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numSamples == 0 || numCh == 0 || oversampler == nullptr)
        return;

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Sécurité : bloc plus grand que prévu
    if (numSamples > preparedBlockSize)
    {
        preparedBlockSize = numSamples;
        oversampler->initProcessing ((size_t) numSamples);
        ensureBuffers (numSamples);
    }

    const int mode = (int) pMode->load();
    if (mode != lastMode)
    {
        for (auto& e : engines) e.reset();
        lastMode = mode;
    }

    crossoverSm.setTargetValue (pCrossover->load());
    amountSm.setTargetValue (pAmount->load() * 0.01f);
    characterSm.setTargetValue (pCharacter->load() * 0.01f);
    mixSm.setTargetValue (pMix->load() * 0.01f);
    outputSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    const bool monoLow = pMonoLow->load() > 0.5f;
    const bool soloLow = pSoloLow->load() > 0.5f;
    const bool cutSub  = pSubCut->load() > 0.5f;

    // 1) Séparation grave / aigu (Linkwitz-Riley 4e ordre, somme à plat)
    for (int i = 0; i < numSamples; ++i)
    {
        if (crossoverSm.isSmoothing() || i == 0)
            crossover.setCutoffFrequency (crossoverSm.getNextValue());

        float lo[2] = { 0.0f, 0.0f }, hi[2] = { 0.0f, 0.0f };
        for (int ch = 0; ch < 2; ++ch)
        {
            const float x = buffer.getSample (juce::jmin (ch, numCh - 1), i);
            crossover.processSample (ch, x, lo[ch], hi[ch]);
        }

        if (monoLow || numCh == 1)
        {
            const float m = 0.5f * (lo[0] + lo[1]);
            lo[0] = lo[1] = m;
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            lowBuffer.setSample (ch, i, lo[ch]);
            dryLowBuffer.setSample (ch, i, lo[ch]);
            highBuffer.setSample (ch, i, hi[ch]);
        }
    }

    // 2) Traitement du grave en suréchantillonné x4
    juce::dsp::AudioBlock<float> lowBlock (lowBuffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
    auto osBlock = oversampler->processSamplesUp (lowBlock);
    const auto osSamples = osBlock.getNumSamples();
    const auto engineMode = (subshaper::Mode) juce::jlimit (0, 4, mode);

    auto* os0 = osBlock.getChannelPointer (0);
    auto* os1 = osBlock.getChannelPointer (1);
    for (size_t i = 0; i < osSamples; ++i)
    {
        const float amt = amountSm.getNextValue();
        const float chr = characterSm.getNextValue();
        os0[i] = engines[0].process (os0[i], engineMode, amt, chr);
        os1[i] = engines[1].process (os1[i], engineMode, amt, chr);
    }

    oversampler->processSamplesDown (lowBlock);

    // 3) Recombinaison (compensation de latence sur le sec et l'aigu)
    for (int i = 0; i < numSamples; ++i)
    {
        const float mix  = mixSm.getNextValue();
        const float gain = outputSm.getNextValue();

        for (int ch = 0; ch < 2; ++ch)
        {
            const float dry = dryLowDelay.popSample (ch);
            dryLowDelay.pushSample (ch, dryLowBuffer.getSample (ch, i));
            const float hi = highDelay.popSample (ch);
            highDelay.pushSample (ch, highBuffer.getSample (ch, i));

            const float wet = lowBuffer.getSample (ch, i);
            float out = dry + mix * (wet - dry);
            if (! soloLow)
                out += hi;
            if (cutSub)
                out = subCut.processSample (ch, out);

            if (ch < numCh)
                buffer.setSample (ch, i, out * gain);
        }
    }

    // 4) Envoi vers l'analyseur
    const auto scope = analyserFifo.write (numSamples);
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

juce::AudioProcessorEditor* SubShaperProcessor::createEditor()
{
    return new SubShaperEditor (*this);
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubShaperProcessor();
}

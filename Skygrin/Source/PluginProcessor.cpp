#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    inline float ceilingClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.85f) return x;
        const float shaped = 0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f);
        return (x < 0.0f) ? -shaped : shaped;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SkygrinAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "intensity", 1 },
        "Intensity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "preset", 1 },
        "Preset",
        getPresetNames(),
        1));

    return layout;
}

//==============================================================================
SkygrinAudioProcessor::SkygrinAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool SkygrinAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void SkygrinAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    spec.numChannels      = 2;

    hpFilter.prepare (spec);
    hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setResonance (0.8f);
    hpFilter.setCutoffFrequency (20.0f);
    hpFilter.reset();

    lpFilter.prepare (spec);
    lpFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lpFilter.setResonance (0.9f);
    lpFilter.setCutoffFrequency (20000.0f);
    lpFilter.reset();

    noiseFilter.prepare (spec);
    noiseFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    noiseFilter.setResonance (2.0f);
    noiseFilter.setCutoffFrequency (400.0f);
    noiseFilter.reset();

    reverb.prepare (spec);
    reverb.reset();

    delayLine.prepare (spec);
    delayLine.reset();

    barber.prepare (spec);
    riser.prepare (sampleRate);

    for (int ch = 0; ch < 2; ++ch)
        shifter[ch].prepare (sampleRate);

    intensitySmoothed.reset (sampleRate, 0.02);
    intensitySmoothed.setCurrentAndTargetValue (
        apvts.getRawParameterValue ("intensity")->load() * 0.01f);

    for (int i = 0; i < NumMods; ++i)
        cur[i] = 0.0f;

    smoothedDelaySamples = (float) (sampleRate * 0.25);
    gatePhase = 0.0;
}

//==============================================================================
void SkygrinAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalIn    = getTotalNumInputChannels();
    const int totalOut   = getTotalNumOutputChannels();

    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, numSamples);

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh <= 0 || numSamples <= 0)
        return;

    double bpm = 128.0;
    if (auto* ph = getPlayHead())
        if (auto position = ph->getPosition())
            if (auto hostBpm = position->getBpm())
                bpm = juce::jlimit (40.0, 220.0, *hostBpm);

    const double beatSeconds = 60.0 / bpm;

    const float target = apvts.getRawParameterValue ("intensity")->load() * 0.01f;
    intensitySmoothed.setTargetValue (target);
    uiIntensity.store (target);

    const int presetIdx = juce::jlimit (0, kNumPresets - 1,
                                        (int) apvts.getRawParameterValue ("preset")->load());
    const PresetDef& P = kPresets[presetIdx];

    juce::Reverb::Parameters rp;
    rp.width      = 1.0f;
    rp.damping    = 0.35f;
    rp.freezeMode = 0.0f;

    int pos = 0;
    while (pos < numSamples)
    {
        const int n = juce::jmin (kChunk, numSamples - pos);
        const float t = intensitySmoothed.skip (n);

        const float coef = 1.0f - std::exp (- (float) n / (float) (currentSr * 0.02));
        for (int i = 0; i < NumMods; ++i)
            cur[i] += (modValue (P, i, t) - cur[i]) * coef;

        const float nyq = (float) (currentSr * 0.45);

        // --- passe-haut resonant : c'est lui qui "chante" quand il monte -----
        hpFilter.setCutoffFrequency (juce::jlimit (20.0f, nyq,
                                     20.0f * std::pow (160.0f, cur[ModHighpass])));
        hpFilter.setResonance (0.7f + cur[ModHighpass] * 2.8f);

        lpFilter.setCutoffFrequency (juce::jlimit (300.0f, nyq,
                                     20000.0f * std::pow (0.025f, cur[ModLowpass])));

        // --- les deux modules qui font monter, tous deux pilotes par t ------
        barber.update (t);
        riser.update (t);

        const float barberMix = cur[ModBarber];
        const float riserGain = cur[ModRiser] * 0.35f;

        //  la bande de bruit suit la meme trajectoire que le riser
        const float noiseGain = cur[ModNoise] * 0.16f;
        const bool  useNoise  = noiseGain > 0.0008f;
        noiseFilter.setCutoffFrequency (juce::jlimit (80.0f, nyq,
                                        (float) riser.getFundamental() * 2.0f));
        noiseFilter.setResonance (1.5f + 6.0f * t);

        const float driveGain = juce::Decibels::decibelsToGain (cur[ModDrive] * 22.0f);
        const float driveComp = std::pow (driveGain, -0.55f);
        const bool  useDrive  = cur[ModDrive] > 0.002f;

        // --- delay qui raccourcit : 1/4 -> 1/8 -> 1/16 ----------------------
        const double division = 1.0 + (double) t * 3.0;        // 1 .. 4
        const float wantedDelay = (float) juce::jlimit (32.0, 190000.0,
                                    currentSr * beatSeconds / division);
        smoothedDelaySamples += (wantedDelay - smoothedDelaySamples) * 0.002f * (float) n;

        const float delayMix = cur[ModDelay] * 0.55f;
        const float feedback = cur[ModFeedback] * 0.70f;
        const float shiftHz  = cur[ModShift] * 55.0f;
        const float shiftWet = juce::jmin (1.0f, cur[ModShift] * 1.2f);

        // --- gate synchro qui s'accelere ------------------------------------
        const float gateDepth = cur[ModGate];
        const double gateDiv = (t < 0.55) ? 2.0 : (t < 0.8 ? 4.0 : 8.0);
        const double gateInc = (gateDiv / beatSeconds) / currentSr;

        const bool useReverb = cur[ModReverb] > 0.002f;
        rp.wetLevel = cur[ModReverb] * 0.50f;
        rp.dryLevel = 1.0f - cur[ModReverb] * 0.18f;
        rp.roomSize = 0.55f + cur[ModReverb] * 0.42f;

        // le niveau MONTE avec l'intensite, il ne baisse plus
        const float outGain = 1.0f + 0.22f * t;

        for (int s = 0; s < n; ++s)
        {
            const int idx = pos + s;

            const float riserSample = (riserGain > 0.0005f)
                                        ? riser.process() * riserGain : 0.0f;

            gatePhase += gateInc;
            if (gatePhase >= 1.0) gatePhase -= 1.0;
            const float gateWin = 0.5f + 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * gatePhase);
            const float gate = 1.0f - gateDepth * (1.0f - gateWin * gateWin);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float x = buffer.getSample (ch, idx);

                x = hpFilter.processSample (ch, x);
                x = lpFilter.processSample (ch, x);

                if (barberMix > 0.002f)
                {
                    const float b = barber.process (ch, x);
                    x = x * (1.0f - barberMix) + b * barberMix;
                }

                x += riserSample;

                if (useNoise)
                {
                    float nz = noiseRng.nextFloat() * 2.0f - 1.0f;
                    nz = noiseFilter.processSample (ch, nz);
                    x += nz * noiseGain;
                }

                // delay dont la boucle de feedback remonte a chaque passage
                const float echo = delayLine.popSample (ch, smoothedDelaySamples, true);
                const float fed  = (shiftWet > 0.002f)
                                    ? shifter[ch].process (echo, shiftHz, shiftWet) : echo;
                delayLine.pushSample (ch, x + fed * feedback);
                x += echo * delayMix;

                if (useDrive)
                    x = softClip (x * driveGain) * driveComp;

                x *= gate * outGain;

                buffer.setSample (ch, idx, x);
            }
        }

        hpFilter.snapToZero();
        lpFilter.snapToZero();
        noiseFilter.snapToZero();
        barber.snapToZero();

        if (useReverb)
        {
            auto block = juce::dsp::AudioBlock<float> (buffer)
                            .getSubsetChannelBlock (0, (size_t) numCh)
                            .getSubBlock ((size_t) pos, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            reverb.setParameters (rp);
            reverb.process (ctx);
        }

        pos += n;
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int s = 0; s < numSamples; ++s)
            d[s] = ceilingClip (d[s]);
    }
}

//==============================================================================
juce::AudioProcessorEditor* SkygrinAudioProcessor::createEditor()
{
    return new SkygrinAudioProcessorEditor (*this);
}

void SkygrinAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SkygrinAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkygrinAudioProcessor();
}

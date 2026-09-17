#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    // plafond doux : ne touche pas le signal tant qu'il reste sous -1 dBFS
    inline float ceilingClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.9f) return x;
        const float over = (a - 0.9f) / 0.1f;
        const float shaped = 0.9f + 0.1f * std::tanh (over);
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
    hpFilter.setResonance (0.85f);
    hpFilter.setCutoffFrequency (20.0f);
    hpFilter.reset();

    lpFilter.prepare (spec);
    lpFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lpFilter.setResonance (0.9f);
    lpFilter.setCutoffFrequency (20000.0f);
    lpFilter.reset();

    noiseFilter.prepare (spec);
    noiseFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    noiseFilter.setResonance (1.0f);
    noiseFilter.setCutoffFrequency (400.0f);
    noiseFilter.reset();

    phaser.prepare (spec);
    phaser.reset();
    phaser.setCentreFrequency (700.0f);
    phaser.setRate (0.4f);
    phaser.setDepth (0.6f);
    phaser.setFeedback (0.0f);
    phaser.setMix (0.0f);

    reverb.prepare (spec);
    reverb.reset();

    delayLine.prepare (spec);
    delayLine.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        shifter[ch].prepare (sampleRate);
        shifter[ch].setPhaseOffset (ch == 0 ? 0.0 : 0.25);
        crusher[ch].reset();
    }

    intensitySmoothed.reset (sampleRate, 0.02);
    intensitySmoothed.setCurrentAndTargetValue (
        apvts.getRawParameterValue ("intensity")->load() * 0.01f);

    for (int i = 0; i < NumMods; ++i)
        cur[i] = 0.0f;

    shiftLfoPhase = 0.0;
}

//==============================================================================
void SkygrinAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
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

    // ---- tempo hote pour caler le delay sur une croche ----------------------
    double bpm = 128.0;
    if (auto* ph = getPlayHead())
        if (auto position = ph->getPosition())
            if (auto hostBpm = position->getBpm())
                bpm = juce::jlimit (40.0, 220.0, *hostBpm);

    const float eighth = (float) (currentSr * (60.0 / bpm) * 0.5);
    delaySamples[0] = juce::jlimit (16.0f, 190000.0f, eighth);
    delaySamples[1] = juce::jlimit (16.0f, 190000.0f, eighth + (float) (currentSr * 0.013));

    // ---- parametres ---------------------------------------------------------
    const float target = apvts.getRawParameterValue ("intensity")->load() * 0.01f;
    intensitySmoothed.setTargetValue (target);
    uiIntensity.store (target);

    const int presetIdx = juce::jlimit (0, kNumPresets - 1,
                                        (int) apvts.getRawParameterValue ("preset")->load());
    const PresetDef& P = kPresets[presetIdx];

    juce::Reverb::Parameters rp;
    rp.width    = 1.0f;
    rp.damping  = 0.35f;
    rp.freezeMode = 0.0f;

    int pos = 0;
    while (pos < numSamples)
    {
        const int n = juce::jmin (kChunk, numSamples - pos);
        const float t = intensitySmoothed.skip (n);

        // lissage des modules (evite les clics au changement de preset)
        const float coef = 1.0f - std::exp (- (float) n / (float) (currentSr * 0.02));
        for (int i = 0; i < NumMods; ++i)
            cur[i] += (modValue (P, i, t) - cur[i]) * coef;

        // ---- mapping vers le DSP -------------------------------------------
        const float nyq = (float) (currentSr * 0.45);

        hpFilter.setCutoffFrequency (juce::jlimit (20.0f, nyq,
                                     20.0f * std::pow (80.0f, cur[ModHighpass])));
        lpFilter.setCutoffFrequency (juce::jlimit (200.0f, nyq,
                                     20000.0f * std::pow (0.019f, cur[ModLowpass])));

        const float driveGain = juce::Decibels::decibelsToGain (cur[ModDrive] * 28.0f);
        const float driveComp = std::pow (driveGain, -0.62f);
        const bool  useDrive  = cur[ModDrive] > 0.001f;

        const bool  useCrush    = cur[ModCrush] > 0.002f;
        const int   crushStep   = 1 + (int) (cur[ModCrush] * 22.0f);
        const float crushLevels = std::pow (2.0f, 16.0f - cur[ModCrush] * 13.0f);

        const bool usePhaser = cur[ModPhaser] > 0.002f;
        if (usePhaser)
        {
            phaser.setRate (0.15f + cur[ModPhaser] * 1.6f);
            phaser.setDepth (0.35f + cur[ModPhaser] * 0.6f);
            phaser.setFeedback (cur[ModPhaser] * 0.6f);
            phaser.setMix (cur[ModPhaser] * 0.9f);
        }

        shiftLfoPhase += (double) n * (0.07 / currentSr);
        while (shiftLfoPhase >= 1.0) shiftLfoPhase -= 1.0;
        const float lfo = (float) std::sin (juce::MathConstants<double>::twoPi * shiftLfoPhase);

        const float shiftHz  = cur[ModShift] * (95.0f + 45.0f * lfo);
        const float shiftWet = juce::jmin (1.0f, cur[ModShift] * 1.15f);
        const bool  useShift = shiftWet > 0.002f;

        const float delayMix = cur[ModDelay] * 0.65f;
        const float feedback = cur[ModFeedback] * 0.72f;

        const float noiseGain = cur[ModNoise] * 0.30f * (0.25f + 0.75f * t);
        const bool  useNoise  = noiseGain > 0.0008f;
        noiseFilter.setCutoffFrequency (juce::jlimit (60.0f, nyq,
                                        180.0f * std::pow (45.0f, t)));
        noiseFilter.setResonance (0.8f + 4.5f * t);

        const bool useReverb = cur[ModReverb] > 0.002f;
        rp.wetLevel = cur[ModReverb] * 0.55f;
        rp.dryLevel = 1.0f - cur[ModReverb] * 0.20f;
        rp.roomSize = 0.55f + cur[ModReverb] * 0.42f;

        const float outGain = 1.0f / (1.0f + 0.30f * t);

        // ---- traitement echantillon par echantillon -------------------------
        for (int s = 0; s < n; ++s)
        {
            const int idx = pos + s;

            for (int ch = 0; ch < numCh; ++ch)
            {
                float x = buffer.getSample (ch, idx);

                if (useNoise)
                {
                    float nz = noiseRng.nextFloat() * 2.0f - 1.0f;
                    nz = noiseFilter.processSample (ch, nz);
                    x += nz * noiseGain;
                }

                if (useDrive)
                    x = softClip (x * driveGain) * driveComp;

                if (useCrush)
                    x = crusher[ch].process (x, crushStep, crushLevels);

                x = hpFilter.processSample (ch, x);
                x = lpFilter.processSample (ch, x);

                if (useShift)
                    x = shifter[ch].process (x, shiftHz, shiftWet);

                const float echo = delayLine.popSample (ch, delaySamples[ch], true);
                delayLine.pushSample (ch, x + echo * feedback);
                x += echo * delayMix;

                buffer.setSample (ch, idx, x * outGain);
            }
        }

        hpFilter.snapToZero();
        lpFilter.snapToZero();
        noiseFilter.snapToZero();

        // ---- modules par blocs ---------------------------------------------
        auto block = juce::dsp::AudioBlock<float> (buffer)
                        .getSubsetChannelBlock (0, (size_t) numCh)
                        .getSubBlock ((size_t) pos, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        if (usePhaser)
            phaser.process (ctx);

        if (useReverb)
        {
            reverb.setParameters (rp);
            reverb.process (ctx);
        }

        pos += n;
    }

    // ---- plafond doux -------------------------------------------------------
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

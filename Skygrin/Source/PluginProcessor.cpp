#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace skygrin;

//==============================================================================
namespace
{
    // Plafond doux final : transparent sous 0,85, arrondi au-dessus.
    inline float ceilingClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.85f) return x;
        const float shaped = 0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f);
        return (x < 0.0f) ? -shaped : shaped;
    }

    // Coefficient d'un lissage exponentiel de constante de temps 'seconds',
    // applique une fois par bloc de 'n' echantillons.
    inline float chunkCoef (int n, double sampleRate, double seconds) noexcept
    {
        return 1.0f - (float) std::exp (-(double) n / (sampleRate * seconds));
    }
}

//==============================================================================
//  PARAMETRES : un potard, un choix de preset. Rien d'autre.
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

    // Les nouveaux presets sont ajoutes en fin de liste : les sessions
    // existantes (index 0..7) rouvrent sur le meme preset.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "preset", 1 },
        "Preset",
        getPresetNames(),
        kDefaultPreset));

    return layout;
}

//==============================================================================
SkygrinAudioProcessor::SkygrinAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    intensityParam = apvts.getRawParameterValue ("intensity");
    presetParam    = apvts.getRawParameterValue ("preset");

    driveChain.get<DriveShaper>().functionToUse = softClip;
}

bool SkygrinAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
//  PREPARE TO PLAY : chaque module recoit la meme ProcessSpec (sauf la
//  saturation, qui tourne a 2x la frequence d'echantillonnage).
//==============================================================================
void SkygrinAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;

    const auto maxBlock = (juce::uint32) juce::jmax (kChunk, samplesPerBlock);
    const juce::dsp::ProcessSpec spec { sampleRate, maxBlock, 2 };

    // ---- filtres ---------------------------------------------------------
    hpFilter.prepare (spec);
    hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency (20.0f);
    hpFilter.setResonance (0.7f);

    lpFilter.prepare (spec);
    lpFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lpFilter.setCutoffFrequency (20000.0f);
    lpFilter.setResonance (0.9f);

    airCoefs = new juce::dsp::IIR::Coefficients<float> (
        juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sampleRate, 4500.0f, 0.707f, 1.0f));
    lastAirDb = 0.0f;
    for (auto& f : airFilter)
    {
        f.coefficients = airCoefs;          // un seul jeu de coefficients pour L et R
        f.prepare (spec);
        f.reset();
    }

    // ---- modules maison --------------------------------------------------
    pitchShifter.prepare (sampleRate, 2);
    barber.prepare (spec);
    noiseRiser.prepare (spec);
    for (auto& s : shifter)
        s.prepare (sampleRate);

    delayLine.prepare (spec);
    delayLine.reset();
    smoothedDelaySamples = (float) (sampleRate * 0.25);
    gatePhase = 0.0;

    // ---- etages par bloc -------------------------------------------------
    reverb.prepare (spec);
    reverb.reset();

    compressor.prepare (spec);
    compressor.setAttack (5.0f);
    compressor.setRelease (120.0f);
    compressor.setThreshold (0.0f);
    compressor.setRatio (1.0f);

    compMakeup.prepare (spec);
    compMakeup.setRampDurationSeconds (0.02);
    compMakeup.setGainLinear (1.0f);

    const auto numCh = (size_t) juce::jlimit (1, 2, getTotalNumOutputChannels());
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        numCh, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing (maxBlock);
    oversampler->reset();

    const auto osFactor = (juce::uint32) oversampler->getOversamplingFactor();
    const juce::dsp::ProcessSpec osSpec { sampleRate * (double) osFactor, maxBlock * osFactor,
                                          (juce::uint32) numCh };
    driveChain.prepare (osSpec);
    driveChain.get<DrivePreGain>().setRampDurationSeconds (0.02);
    driveChain.get<DrivePostGain>().setRampDurationSeconds (0.02);
    driveChain.reset();

    driveMixer.prepare (osSpec);
    driveMixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    driveMixer.setWetMixProportion (0.0f);
    driveMixer.reset();

    // Le sur-echantillonnage IIR ajoute quelques echantillons de latence :
    // on la declare a l'hote pour qu'il la compense.
    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));

    // ---- controle ----------------------------------------------------------
    intensitySmoothed.reset (sampleRate, 0.02);
    intensitySmoothed.setCurrentAndTargetValue (intensityParam->load() * 0.01f);

    std::fill (std::begin (mods), std::end (mods), 0.0f);
    inMs = postMs = 0.0f;
    lossMakeupDb = 0.0f;
    lossMakeupGain = 1.0f;
}

//==============================================================================
//  MAPPING : position du potard -> reglages physiques de chaque sous-effet.
//  'mods' = sorties lissees des courbes du preset (0..1).
//==============================================================================
float SkygrinAudioProcessor::delayNoteValue (const PresetDef& p, float t) noexcept
{
    // Divisions synchro tempo par paliers (1/4 -> 1/8 -> 1/16...), repartis
    // entre 25 % et 100 % du potard.
    const int from  = juce::jmax (1, p.delayFrom);
    const int steps = juce::jmax (0, juce::roundToInt (std::log2 ((float) juce::jmax (from, p.delayTo) / (float) from)));
    const float x   = juce::jlimit (0.0f, 1.0f, (t - 0.25f) / 0.75f);
    const int k     = juce::jmin (steps, (int) (x * (float) (steps + 1)));
    return (float) (from << k);
}

SkygrinAudioProcessor::FxSettings SkygrinAudioProcessor::mapToSettings (const float* m, float t,
                                                                         const PresetDef& p,
                                                                         double bpm,
                                                                         double sampleRate) noexcept
{
    FxSettings fx;

    const float  nyq        = (float) (sampleRate * 0.45);
    const double beatSec    = 60.0 / bpm;

    // ---- 1. Filtres ------------------------------------------------------
    //  Passe-haut : 20 Hz -> 3,2 kHz en domaine log (0.62 = 466 Hz : kick et
    //  basse ont disparu). Sa resonance monte avec lui : c'est lui qui chante.
    fx.hpHz  = juce::jlimit (20.0f, nyq, logMap (m[ModHighpass], 20.0f, 3200.0f));
    fx.hpRes = 0.7f + 2.8f * m[ModHighpass];

    //  Passe-bas : ouvert (20 kHz) sauf presets "tunnel" qui referment le haut.
    fx.lpHz  = juce::jlimit (300.0f, nyq, logMap (m[ModLowpass], 20000.0f, 500.0f));

    //  Air : shelf aigu, la brillance au sommet de la montee.
    fx.airDb = 9.0f * m[ModAir];

    //  Compensation de la perte de grave : active des que le passe-haut monte.
    fx.lossComp = fadeIn (m[ModHighpass], 0.10f);

    // ---- 2. Pitch --------------------------------------------------------
    fx.pitchSemis = 12.0f * m[ModPitch];
    fx.pitchWet   = p.pitchWet * fadeIn (fx.pitchSemis, 0.25f);

    // ---- 3. Barber pole + souffle ----------------------------------------
    fx.barberMix  = m[ModBarber];
    fx.noiseSweep = m[ModNoise] * 0.27f;
    fx.noiseBed   = m[ModNoise] * 0.13f;

    // ---- 4. Delay synchro tempo ------------------------------------------
    const float note  = delayNoteValue (p, t);
    fx.delaySamples   = (float) juce::jlimit (32.0, 190000.0, sampleRate * beatSec * 4.0 / (double) note);
    fx.delayMix       = m[ModDelay]    * 0.55f;
    fx.feedback       = m[ModFeedback] * 0.70f;
    fx.shiftHz        = m[ModShift]    * 55.0f;
    fx.shiftWet       = juce::jmin (1.0f, m[ModShift] * 1.2f);

    // ---- 5. Gate synchro qui accelere : 1/8 -> 1/16 -> 1/32 --------------
    fx.gateDepth = m[ModGate];
    const double gateDiv = (t < 0.55f) ? 2.0 : (t < 0.8f ? 4.0 : 8.0);
    fx.gateInc   = (gateDiv / beatSec) / sampleRate;

    // ---- 6. Reverbe : plus de mix, plus grande, plus longue, plus claire --
    //  Attention : juce::Reverb multiplie dryLevel par 2 (et wetLevel par 3).
    //  dryLevel = 0,5 -> sec a gain unite. La v0.5 envoyait 1,0 : +6 dB d'un
    //  coup des que la reverbe s'activait (au moindre mouvement du potard).
    const float r   = m[ModReverb];
    fx.reverbWet    = 0.40f * r;
    fx.reverbDry    = 0.5f * (1.0f - 0.30f * r);
    fx.reverbRoom   = 0.55f + 0.43f * r;
    fx.reverbDamp   = 0.45f - 0.30f * r;

    // ---- 7. Compresseur : seuil qui descend, ratio qui monte -------------
    const float c    = m[ModComp];
    fx.compThreshDb  = -24.0f * c;
    fx.compRatio     = 1.0f + 5.0f * c;
    fx.compMakeupDb  = -fx.compThreshDb * (1.0f - 1.0f / fx.compRatio) * 0.5f;

    // ---- 8. Saturation ---------------------------------------------------
    fx.driveDb    = 22.0f * m[ModDrive];
    fx.driveBlend = fadeIn (m[ModDrive], 0.05f);    // 0 % = parfaitement sec

    // ---- 9. Le niveau monte avec l'intensite, il ne baisse jamais --------
    fx.outGain = 1.0f + 0.22f * t;

    return fx;
}

//==============================================================================
//  Envoie les reglages du bloc courant aux modules.
//==============================================================================
void SkygrinAudioProcessor::updateModules (const FxSettings& fx, float t, int n)
{
    hpFilter.setCutoffFrequency (fx.hpHz);
    hpFilter.setResonance (fx.hpRes);
    lpFilter.setCutoffFrequency (fx.lpHz);

    if (std::abs (fx.airDb - lastAirDb) > 0.01f)
    {
        // ArrayCoefficients : aucun alloc dans le thread audio.
        *airCoefs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
                        currentSr, 4500.0f, 0.707f, dbToGain (fx.airDb));
        lastAirDb = fx.airDb;
    }

    pitchShifter.setSemitones (fx.pitchSemis);
    barber.update (t);
    noiseRiser.update (t);

    // Le temps de delay glisse (40 ms) quand la division change : petit "swoop".
    smoothedDelaySamples += (fx.delaySamples - smoothedDelaySamples) * chunkCoef (n, currentSr, 0.04);

    // Compensation du grave perdu : 50 % de la perte d'energie, 6 dB maximum.
    if (inMs > 1.0e-7f)
    {
        const float lossDb = 10.0f * std::log10 ((inMs + 1.0e-12f) / (postMs + 1.0e-12f));
        const float target = juce::jlimit (0.0f, 6.0f, 0.5f * lossDb) * fx.lossComp;
        lossMakeupDb += (target - lossMakeupDb) * chunkCoef (n, currentSr, 0.15);
    }
    lossMakeupGain = dbToGain (lossMakeupDb);

    juce::dsp::Reverb::Parameters rp;
    rp.wetLevel   = fx.reverbWet;
    rp.dryLevel   = fx.reverbDry;
    rp.roomSize   = fx.reverbRoom;
    rp.damping    = fx.reverbDamp;
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    compressor.setThreshold (fx.compThreshDb);
    compressor.setRatio (fx.compRatio);
    compMakeup.setGainDecibels (fx.compMakeupDb);

    const float driveGain = dbToGain (fx.driveDb);
    driveChain.get<DrivePreGain>().setGainLinear (driveGain);
    driveChain.get<DrivePostGain>().setGainLinear (std::pow (driveGain, -0.55f));
    driveMixer.setWetMixProportion (fx.driveBlend);
}

//==============================================================================
//  ETAGES ECHANTILLON PAR ECHANTILLON (filtres, pitch, barber, souffle,
//  delay, gate) : ils ont besoin d'une boucle par echantillon (feedback,
//  phase du gate, tetes du pitch shifter).
//==============================================================================
void SkygrinAudioProcessor::processSampleStages (juce::AudioBuffer<float>& buffer,
                                                 int start, int n, int numCh,
                                                 const FxSettings& fx)
{
    const bool useBarber = fx.barberMix > 0.002f;
    const bool useNoise  = (fx.noiseSweep + fx.noiseBed) > 0.0005f;
    const bool useShift  = fx.shiftWet > 0.002f;

    // Melange du pitch a puissance constante : le signal transpose n'est plus
    // correle au signal sec, un fondu lineaire creuserait le niveau (-3 dB a 50 %).
    // +1 dB sur le transpose : compense la perte moyenne des fenetres sin^2.
    const float pitchDryGain = std::cos (fx.pitchWet * juce::MathConstants<float>::halfPi);
    const float pitchWetGain = std::sin (fx.pitchWet * juce::MathConstants<float>::halfPi) * 1.12f;

    const float envCoef = 1.0f - std::exp (-1.0f / (float) (currentSr * 0.3));   // 300 ms
    const float chanNorm = 1.0f / (float) numCh;

    for (int s = 0; s < n; ++s)
    {
        const int idx = start + s;

        // gate : fenetre en cosinus carre, jamais de coupure seche
        gatePhase += fx.gateInc;
        if (gatePhase >= 1.0) gatePhase -= 1.0;
        const float win  = 0.5f + 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * gatePhase);
        const float gate = 1.0f - fx.gateDepth * (1.0f - win * win);

        float inPow = 0.0f, postPow = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float x = buffer.getSample (ch, idx);
            inPow += x * x;

            // 1. filtres
            x = hpFilter.processSample (ch, x);
            x = lpFilter.processSample (ch, x);
            postPow += x * x;
            x = airFilter[ch].processSample (x);

            // 2. le grave est parti : on remonte le reste
            x *= lossMakeupGain;

            // 3. pitch (toujours ecrit dans son tampon : pas de trou a l'activation)
            const float shifted = pitchShifter.process (ch, x);
            x = x * pitchDryGain + shifted * pitchWetGain;

            // 4. barber pole
            if (useBarber)
                x += (barber.process (ch, x) - x) * fx.barberMix;

            // 5. souffle
            if (useNoise)
                x += noiseRiser.process (ch, noiseRng.nextFloat() * 2.0f - 1.0f,
                                         fx.noiseSweep, fx.noiseBed);

            // 6. delay : chaque repetition remonte (frequency shifter dans la boucle)
            const float echo = delayLine.popSample (ch, smoothedDelaySamples, true);
            const float fed  = useShift ? shifter[ch].process (echo, fx.shiftHz, fx.shiftWet) : echo;
            delayLine.pushSample (ch, x + fed * fx.feedback);
            x += echo * fx.delayMix;

            // 7. gate
            buffer.setSample (ch, idx, x * gate);
        }

        pitchShifter.advance();

        inMs   += (inPow   * chanNorm - inMs)   * envCoef;
        postMs += (postPow * chanNorm - postMs) * envCoef;
    }

    hpFilter.snapToZero();
    lpFilter.snapToZero();
    for (auto& f : airFilter) f.snapToZero();
    noiseRiser.snapToZero();
    barber.snapToZero();
}

//==============================================================================
//  ETAGES PAR BLOC : modules juce::dsp standards (ProcessContextReplacing).
//==============================================================================
void SkygrinAudioProcessor::processBlockStages (juce::dsp::AudioBlock<float>& block,
                                                const FxSettings& fx)
{
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // Reverbe : coupee quand elle est a zero (economie CPU, aucun son).
    if (fx.reverbWet > 0.002f)
        reverb.process (ctx);

    // Compresseur (ratio 1:1 a 0 % = transparent) puis makeup.
    compressor.process (ctx);
    compMakeup.process (ctx);

    // Saturation sur-echantillonnee : up -> drive/tanh/retour -> mix -> down.
    auto osBlock = oversampler->processSamplesUp (block);
    driveMixer.pushDrySamples (osBlock);
    driveChain.process (juce::dsp::ProcessContextReplacing<float> (osBlock));
    driveMixer.mixWetSamples (osBlock);
    oversampler->processSamplesDown (block);

    block.multiplyBy (fx.outGain);
}

//==============================================================================
//  PROCESS BLOCK
//==============================================================================
void SkygrinAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh <= 0 || numSamples <= 0)
        return;

    // Tempo de l'hote (128 BPM si l'hote n'en donne pas).
    double bpm = 128.0;
    if (auto* ph = getPlayHead())
        if (auto position = ph->getPosition())
            if (auto hostBpm = position->getBpm())
                bpm = juce::jlimit (40.0, 220.0, *hostBpm);

    const float target = intensityParam->load() * 0.01f;
    intensitySmoothed.setTargetValue (target);
    uiIntensity.store (target);

    const int presetIdx = juce::jlimit (0, kNumPresets - 1, (int) presetParam->load());
    const PresetDef& preset = kPresets[presetIdx];

    auto fullBlock = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) numCh);

    for (int pos = 0; pos < numSamples; pos += kChunk)
    {
        const int n = juce::jmin (kChunk, numSamples - pos);
        const float t = intensitySmoothed.skip (n);

        // 1. courbes du preset -> valeurs lissees (20 ms) -> unites physiques
        const float coef = chunkCoef (n, currentSr, 0.02);
        for (int i = 0; i < NumMods; ++i)
            mods[i] += (modValue (preset, i, t) - mods[i]) * coef;

        const FxSettings fx = mapToSettings (mods, t, preset, bpm, currentSr);
        updateModules (fx, t, n);

        // 2. etages echantillon par echantillon
        processSampleStages (buffer, pos, n, numCh, fx);

        // 3. etages par bloc
        auto sub = fullBlock.getSubBlock ((size_t) pos, (size_t) n);
        processBlockStages (sub, fx);
    }

    // 4. plafond doux
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

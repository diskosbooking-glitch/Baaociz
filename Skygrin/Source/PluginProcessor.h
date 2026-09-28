#pragma once

#include <JuceHeader.h>
#include "Presets.h"
#include "DspModules.h"

//==============================================================================
//  SKYGRIN v0.6 - AUDIO PROCESSOR
//
//  Chaine (dans l'ordre du signal) :
//
//   par echantillon  : passe-haut -> passe-bas -> air (shelf) -> compensation
//                      du grave perdu -> pitch shifter -> barber pole -> + souffle
//                      -> delay synchro (frequency shifter dans le feedback) -> gate
//   par bloc         : reverbe -> compresseur (+ makeup) -> saturation tanh
//                      sur-echantillonnee x2 -> gain de sortie -> plafond doux
//
//  Controle : toutes les 32 echantillons, Intensity passe par les courbes du
//  preset (Presets.h), les valeurs sont lissees, puis converties en unites
//  physiques par mapToSettings(). Les modules ne voient jamais Intensity.
//==============================================================================
class SkygrinAudioProcessor : public juce::AudioProcessor
{
public:
    SkygrinAudioProcessor();
    ~SkygrinAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;       // version double : celle de JUCE

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                            { return true; }

    const juce::String getName() const override                { return "Skygrin"; }
    bool acceptsMidi() const override                          { return false; }
    bool producesMidi() const override                         { return false; }
    bool isMidiEffect() const override                         { return false; }
    double getTailLengthSeconds() const override               { return 6.0; }

    int getNumPrograms() override                              { return 1; }
    int getCurrentProgram() override                           { return 0; }
    void setCurrentProgram (int) override                      {}
    const juce::String getProgramName (int) override           { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiIntensity { 0.0f };      // lu par l'interface (animation)

    //==========================================================================
    //  Reglages de TOUS les sous-effets, en unites physiques, pour une
    //  position du potard. Structure simple : facile a tester hors DAW.
    //==========================================================================
    struct FxSettings
    {
        float hpHz = 20.0f, hpRes = 0.7f, lpHz = 20000.0f, airDb = 0.0f;
        float lossComp = 0.0f;                      // part du grave perdu a compenser
        float pitchSemis = 0.0f, pitchWet = 0.0f;
        float barberMix = 0.0f, noiseSweep = 0.0f, noiseBed = 0.0f;
        float delaySamples = 0.0f, delayMix = 0.0f, feedback = 0.0f;
        float shiftHz = 0.0f, shiftWet = 0.0f;
        float gateDepth = 0.0f;
        double gateInc = 0.0;                       // cycles de gate par echantillon
        float reverbWet = 0.0f, reverbDry = 0.5f, reverbRoom = 0.55f, reverbDamp = 0.45f;
        float compThreshDb = 0.0f, compRatio = 1.0f, compMakeupDb = 0.0f;
        float driveDb = 0.0f, driveBlend = 0.0f;
        float outGain = 1.0f;
    };

    static FxSettings mapToSettings (const float* mods, float t, const PresetDef& preset,
                                     double bpm, double sampleRate) noexcept;

    static float delayNoteValue (const PresetDef& preset, float t) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void updateModules (const FxSettings& fx, float t, int numSamples);
    void processSampleStages (juce::AudioBuffer<float>&, int start, int num, int numCh,
                              const FxSettings& fx);
    void processBlockStages (juce::dsp::AudioBlock<float>& block, const FxSettings& fx);

    //==========================================================================
    static constexpr int kChunk = 32;               // periode du controle (echantillons)

    std::atomic<float>* intensityParam = nullptr;
    std::atomic<float>* presetParam    = nullptr;

    double currentSr = 44100.0;

    juce::SmoothedValue<float> intensitySmoothed;
    float mods[NumMods] = {};                       // valeurs des courbes, lissees

    // ---- etages echantillon par echantillon ---------------------------------
    juce::dsp::StateVariableTPTFilter<float> hpFilter, lpFilter;
    juce::dsp::IIR::Filter<float> airFilter[2];
    juce::dsp::IIR::Coefficients<float>::Ptr airCoefs;
    float lastAirDb = -1.0f;

    PitchShifter pitchShifter;
    BarberFilter barber;
    NoiseRiser   noiseRiser;
    juce::Random noiseRng;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { 192000 };
    FreqShifter shifter[2];
    float smoothedDelaySamples = 12000.0f;

    double gatePhase = 0.0;

    // compensation de la perte de grave (suiveurs d'energie avant / apres filtres)
    float inMs = 0.0f, postMs = 0.0f, lossMakeupDb = 0.0f, lossMakeupGain = 1.0f;

    // ---- etages par bloc ----------------------------------------------------
    juce::dsp::Reverb reverb;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> compMakeup;

    // Saturation : Gain (drive) -> WaveShaper (tanh) -> Gain (retour de niveau),
    // melangee au signal sec par un DryWetMixer, le tout en x2.
    // Cree dans prepareToPlay avec le nombre de canaux reel (mono ou stereo).
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::ProcessorChain<juce::dsp::Gain<float>,
                              juce::dsp::WaveShaper<float>,
                              juce::dsp::Gain<float>> driveChain;
    juce::dsp::DryWetMixer<float> driveMixer;

    enum { DrivePreGain = 0, DriveShaper, DrivePostGain };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkygrinAudioProcessor)
};

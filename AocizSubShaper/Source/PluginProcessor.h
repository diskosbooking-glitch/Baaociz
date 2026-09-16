#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <unordered_map>
#include <string>
#include "BassEngine.h"
#include "Params.h"
#include "Presets.h"

class SubShaperProcessor : public juce::AudioProcessor
{
public:
    SubShaperProcessor();
    ~SubShaperProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Subshaper"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    // Presets usine exposés au DAW
    int getNumPrograms() override { return (int) presets::factory().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadFactoryPreset (int index);

    // --- Presets utilisateur ---
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String getPresetName() const { return apvts.state.getProperty ("presetName", "Init").toString(); }

    // --- A/B ---
    void selectSlot (int slot);
    void copyToOtherSlot();
    int getActiveSlot() const { return activeSlot; }

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // --- Données pour l'interface ---
    static constexpr int fifoSize = 16384;
    juce::AbstractFifo analyserFifo { fifoSize };
    std::array<float, fifoSize> analyserData {};
    juce::AbstractFifo tunerFifo { fifoSize };
    std::array<float, fifoSize> tunerData {};
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };
    std::atomic<double> hostBpm { 120.0 };

    float getKeyFrequency() const;

private:
    void ensureBuffers (int numSamples);
    void configureQuality (bool hq);

    std::unique_ptr<juce::dsp::Oversampling<float>> osHQ, osEco;
    juce::dsp::Oversampling<float>* oversampler = nullptr;
    bool activeHQ = true;
    int latency = 0;

    juce::dsp::LinkwitzRileyFilter<float> crossover;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryLowDelay { 4096 }, highDelay { 4096 };
    juce::dsp::StateVariableTPTFilter<float> subCut;
    std::array<subshaper::ChannelEngine, 2> engines;
    std::array<subshaper::TransientShaper, 2> shapers;

    juce::AudioBuffer<float> lowBuffer, dryLowBuffer, highBuffer;
    int preparedBlockSize = 0;
    double baseRate = 44100.0;

    // Lissages (fréquence de base)
    juce::SmoothedValue<float> inGainSm, crossoverSm, mixSm, outputSm, attackSm, sustainSm,
                               shapeEnSm, pumpEnSm, pumpDepthSm, widthSm, widthEnSm;
    // Lissages (fréquence suréchantillonnée)
    juce::SmoothedValue<float> keyFreqSm, genEnSm, genLevelSm, genToneSm, toneEnSm, toneAmtSm, toneQSm,
                               driveEnSm, driveSm, colorSm, focusSm, driveMixSm;
    int lastGenType = -1;

    // Pompe
    double freePhaseBeats = 0.0;

    // Égalisation de volume
    float msIn = 0.0f, msOut = 0.0f, matchGain = 1.0f;

    // Tuner (décimation x4)
    float tunerAccum = 0.0f;
    int tunerCount = 0;

    // A/B
    juce::ValueTree slots[2];
    int activeSlot = 0;
    int currentProgram = 0;

    std::unordered_map<std::string, std::atomic<float>*> raw;
    float p (const char* id) const { return raw.at (id)->load(); }
    bool  b (const char* id) const { return raw.at (id)->load() > 0.5f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubShaperProcessor)
};

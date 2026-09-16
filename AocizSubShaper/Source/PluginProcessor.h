#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "BassEngine.h"

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
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // --- FIFO pour l'analyseur (écrit par l'audio, lu par l'interface) ---
    static constexpr int fifoSize = 16384;
    juce::AbstractFifo analyserFifo { fifoSize };
    std::array<float, fifoSize> analyserData {};
    double getCurrentSampleRate() const { return currentSampleRate.load(); }

private:
    void ensureBuffers (int numSamples);

    static constexpr int osFactorLog2 = 2; // x4
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::LinkwitzRileyFilter<float> crossover;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryLowDelay { 4096 }, highDelay { 4096 };
    juce::dsp::StateVariableTPTFilter<float> subCut;
    std::array<subshaper::ChannelEngine, 2> engines;

    juce::AudioBuffer<float> lowBuffer, dryLowBuffer, highBuffer;
    int latency = 0;
    int preparedBlockSize = 0;
    std::atomic<double> currentSampleRate { 44100.0 };

    juce::SmoothedValue<float> crossoverSm, amountSm, characterSm, mixSm, outputSm;
    int lastMode = -1;

    std::atomic<float>* pCrossover = nullptr, *pAmount = nullptr, *pCharacter = nullptr,
                      * pMix = nullptr, *pOutput = nullptr, *pMode = nullptr,
                      * pMonoLow = nullptr, *pSoloLow = nullptr, *pSubCut = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubShaperProcessor)
};

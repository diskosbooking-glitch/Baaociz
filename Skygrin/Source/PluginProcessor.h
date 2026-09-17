#pragma once

#include <JuceHeader.h>
#include "Presets.h"
#include "DspModules.h"

class SkygrinAudioProcessor : public juce::AudioProcessor
{
public:
    SkygrinAudioProcessor();
    ~SkygrinAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                            { return true; }

    const juce::String getName() const override                { return "Skygrin"; }
    bool acceptsMidi() const override                          { return false; }
    bool producesMidi() const override                         { return false; }
    bool isMidiEffect() const override                         { return false; }
    double getTailLengthSeconds() const override               { return 4.0; }

    int getNumPrograms() override                              { return 1; }
    int getCurrentProgram() override                           { return 0; }
    void setCurrentProgram (int) override                      {}
    const juce::String getProgramName (int) override           { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> uiIntensity { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kChunk = 32;

    double currentSr = 44100.0;

    juce::SmoothedValue<float> intensitySmoothed;
    float cur[NumMods] = { 0.0f };

    juce::dsp::StateVariableTPTFilter<float> hpFilter, lpFilter, noiseFilter;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { 192000 };

    BarberFilter barber;
    Riser        riser;
    FreqShifter  shifter[2];
    juce::Random noiseRng;

    float  smoothedDelaySamples = 12000.0f;
    double gatePhase = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkygrinAudioProcessor)
};

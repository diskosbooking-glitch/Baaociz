#pragma once
#include <JuceHeader.h>
#include "MementoEngine.h"

// ===========================================================================
// Memento — instrument AU (v0.1). Génère un "song starter" assemblé depuis un
// dossier de samples, calé automatiquement sur le tempo du projet hôte.
// ===========================================================================

class MementoAudioProcessor : public juce::AudioProcessor
{
public:
    MementoAudioProcessor();
    ~MementoAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Memento"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    mem::MementoEngine& getEngine() { return engine; }

private:
    mem::MementoEngine engine;
    juce::int64 freePos = 0;      // horloge de repli si l'hôte ne donne pas de position

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MementoAudioProcessor)
};

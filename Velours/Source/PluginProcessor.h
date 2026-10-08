#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "Params.h"
#include "Bands.h"
#include "Presets.h"
#include "ResonanceEngine.h"

class VeloursProcessor : public juce::AudioProcessor
{
public:
    VeloursProcessor();
    ~VeloursProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Velours"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    int getNumPrograms() override { return (int) presets::factory().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override { loadFactoryPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadFactoryPreset (int index);
    juce::String getPresetName() const { return apvts.state.getProperty ("presetName", "Init").toString(); }

    // --- Presets utilisateur (~/Library/Application Support/Velours/Presets) ---
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);

    // --- A/B ---
    void selectSlot (int slot);
    void copyToOtherSlot();
    int getActiveSlot() const { return activeSlot; }

    juce::UndoManager undoManager;

    // Le réglage courant diffère-t-il du dernier preset chargé ?
    bool isPresetModified() const;
    void capturePresetSnapshot();

    // Bandes courantes (lecture interface)
    std::array<bands::Band, params::numBands> readBands() const;

    juce::AudioProcessorValueTreeState apvts;
    velours::Engine engine;

    // BAND LISTEN : bande écoutée (-1 = aucune), réglée par l'écran pendant un glisser
    std::atomic<int> listenBand { -1 };
    std::atomic<bool> bandListenOnDrag { false };

    std::atomic<bool> sidechainConnected { false };
    std::atomic<int> processChannels { 2 };
    std::atomic<double> currentRate { 48000.0 };

private:
    velours::Settings readSettings() const;
    void updateWeights (int numChannels, bool midSide, bool force);
    void updateListenMask (int band);

    std::array<bands::Band, params::numBands> lastBands {};
    int lastWeightChannels = -1, lastWeightBins = -1;
    bool lastWeightMS = false;
    bands::Band lastListen {};
    int lastListenBand = -2, lastListenBins = -1;

    juce::AudioParameterBool* bypassParam = nullptr;
    int currentProgram = 0;
    juce::ValueTree slots[2];
    std::vector<float> presetSnapshot;
    int activeSlot = 0;
    void applyStateValues (const juce::ValueTree& tree);

    struct BandParams { std::atomic<float>* on; std::atomic<float>* type; std::atomic<float>* freq;
                        std::atomic<float>* gain; std::atomic<float>* q; std::atomic<float>* focus; std::atomic<float>* byp; };
    std::array<BandParams, params::numBands> bandRaw {};
    std::atomic<float>* raw (const char* id) const { return apvts.getRawParameterValue (id); }
    std::atomic<float> *pDepth, *pDetail, *pAttack, *pRelease, *pMaxCut, *pMode, *pDetailTilt, *pTimeTilt,
                       *pStereo, *pLink, *pSidechain, *pMix, *pWetTrim, *pOutput, *pDelta, *pBypass, *pQuality, *pReleaseTilt,
                       *pTimeQuality, *pRenderUltra, *pFocus, *pGainMatch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VeloursProcessor)
};

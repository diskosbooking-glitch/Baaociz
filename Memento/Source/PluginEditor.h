#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ===========================================================================
// Interface v0.1 — fonctionnelle. Le look Miró / Upcycle viendra ensuite.
// ===========================================================================

class MementoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit MementoAudioProcessorEditor (MementoAudioProcessor&);
    ~MementoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildRows();
    void refreshRows();
    void chooseFolder();

    // Une ligne = un slot.
    struct SlotRow : public juce::Component
    {
        SlotRow (MementoAudioProcessor& p, int index);
        void resized() override;
        void refresh();

        MementoAudioProcessor& proc;
        int slotIndex;
        juce::Label   roleLabel, nameLabel, subLabel;
        juce::TextButton rerollBtn { "Reroll" };
        juce::TextButton lockBtn   { "Lock" };
        juce::TextButton muteBtn   { "M" };
        juce::TextButton soloBtn   { "S" };
        juce::Slider  volSlider;
    };

    MementoAudioProcessor& processor;

    juce::Label   titleLabel, bpmLabel, counterLabel, folderLabel;
    juce::TextButton folderBtn   { "Choisir le dossier…" };
    juce::TextButton rerollAllBtn { "Reroll ALL" };
    juce::Viewport viewport;
    juce::Component rowsHolder;
    juce::OwnedArray<SlotRow> rows;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MementoAudioProcessorEditor)
};

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ===========================================================================
// Interface Memento v0.2 — refonte "carte par slot", chips de couleur par
// rôle, export stems (par slot + global) et générateur de rolls par style.
// ===========================================================================

class MementoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit MementoAudioProcessorEditor (MementoAudioProcessor&);
    ~MementoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Actions déclenchées depuis les lignes / la barre d'outils.
    void exportSlotStem (int slotIndex);
    void requestRemoveSlot (int slotIndex);

private:
    void timerCallback() override;
    void rebuildRows();
    void refreshRows();
    void refreshStyles();
    void chooseFolder();
    void chooseStylesFolder();
    void exportAllStems();
    void generateRoll();
    void addStyleLoop();
    void setStatus (const juce::String& text, int ticks = 24);
    juce::String currentStyle() const;

    // Une ligne = un slot (carte).
    struct SlotRow : public juce::Component
    {
        SlotRow (MementoAudioProcessor& p, MementoAudioProcessorEditor& o, int index);
        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

        MementoAudioProcessor&        proc;
        MementoAudioProcessorEditor&  owner;
        int slotIndex;
        juce::Colour chip;
        juce::Label   roleLabel, nameLabel, subLabel;
        juce::TextButton rerollBtn { "Reroll" };
        juce::TextButton lockBtn   { "Lock" };
        juce::TextButton muteBtn   { "M" };
        juce::TextButton soloBtn   { "S" };
        juce::TextButton stemsBtn  { "STEMS" };
        juce::TextButton removeBtn { juce::String::fromUTF8 ("\xC3\x97") }; // ×
        juce::Slider  volSlider;
    };

    MementoAudioProcessor& processor;
    juce::LookAndFeel_V4   lnf;

    // Header + barre d'outils.
    juce::Label   titleLabel, subtitleLabel, bpmLabel, counterLabel, folderLabel, statusLabel;
    juce::TextButton folderBtn    { juce::String::fromUTF8 ("Dossier samples\xE2\x80\xA6") };
    juce::TextButton rerollAllBtn { "Reroll ALL" };
    juce::TextButton exportAllBtn { "EXPORT ALL STEMS" };
    juce::TextButton stylesBtn    { juce::String::fromUTF8 ("STYLES\xE2\x80\xA6") };
    juce::TextButton genRollBtn   { "GENERATE ROLL" };
    juce::TextButton loopStyleBtn { "+ LOOP STYLE" };
    juce::ComboBox   styleCombo;

    juce::Viewport viewport;
    juce::Component rowsHolder;
    juce::OwnedArray<SlotRow> rows;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String lastStyleSig;
    int statusCountdown = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MementoAudioProcessorEditor)
};

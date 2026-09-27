#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ===========================================================================
// Interface Memento v0.3 — carte par slot, export stems (par slot + global),
// glisser-déposer d'un stem directement dans le DAW, générateur de rolls par
// style, et accordage : PROJECT KEY + KEY SYNC (global) + tune par slot
// (Auto / Original / Manual).
// ===========================================================================

class MementoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public  juce::DragAndDropContainer,
                                    private juce::Timer
{
public:
    explicit MementoAudioProcessorEditor (MementoAudioProcessor&);
    ~MementoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Actions déclenchées depuis les lignes / la barre d'outils.
    void exportSlotStem (int slotIndex);      // clic STEMS → dossier de destination
    void startStemDrag  (int slotIndex);      // glisser STEMS → fichier temp → DAW
    void requestRemoveSlot (int slotIndex);

    // Bouton STEMS : clic = export dossier, glisser = drag & drop vers le DAW.
    struct StemControl : public juce::Component
    {
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp   (const juce::MouseEvent&) override;

        MementoAudioProcessorEditor* owner = nullptr;
        int  slotIndex = 0;
        bool dragging  = false;
    };

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
    void buildProjectKeyBox();
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
        StemControl      stems;
        juce::ComboBox   tuneBox;
        juce::TextButton removeBtn { juce::String::fromUTF8 ("\xC3\x97") }; // ×
        juce::Slider  volSlider;
        bool tuneBuilt = false;
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

    // Accordage global.
    juce::Label      keyLabel;
    juce::ComboBox   projectKeyBox;
    juce::TextButton keySyncBtn { "KEY SYNC" };

    juce::Viewport viewport;
    juce::Component rowsHolder;
    juce::OwnedArray<SlotRow> rows;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String lastStyleSig;
    int statusCountdown = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MementoAudioProcessorEditor)
};

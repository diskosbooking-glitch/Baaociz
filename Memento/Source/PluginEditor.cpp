#include "PluginEditor.h"

using namespace juce;

static const Colour kPaper   = Colour (0xfffbeeda);
static const Colour kInk     = Colour (0xff1c160e);
static const Colour kPink    = Colour (0xfff45d9a);
static const Colour kBlue    = Colour (0xff3b82d6);
static const Colour kYellow  = Colour (0xfff5cf4b);
static const Colour kMint    = Colour (0xff8fe3cc);

// ---------------------------------------------------------------------------
// SlotRow
// ---------------------------------------------------------------------------
MementoAudioProcessorEditor::SlotRow::SlotRow (MementoAudioProcessor& p, int index)
    : proc (p), slotIndex (index)
{
    auto setLbl = [] (Label& l, float sz, Justification j, Colour c) {
        l.setFont (Font (sz, Font::bold)); l.setColour (Label::textColourId, c); l.setJustificationType (j);
    };
    setLbl (roleLabel, 12.0f, Justification::centredLeft, kInk);
    setLbl (nameLabel, 14.0f, Justification::centredLeft, kInk);
    setLbl (subLabel,  11.0f, Justification::centredLeft, Colour (0xff5f5443));
    addAndMakeVisible (roleLabel);
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (subLabel);

    rerollBtn.onClick = [this] { proc.getEngine().rerollSlot (slotIndex); };
    addAndMakeVisible (rerollBtn);

    for (auto* b : { &lockBtn, &muteBtn, &soloBtn }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
    lockBtn.onClick = [this] { if (auto* s = proc.getEngine().getSlot (slotIndex)) s->locked.store (lockBtn.getToggleState()); };
    muteBtn.onClick = [this] { if (auto* s = proc.getEngine().getSlot (slotIndex)) s->mute.store   (muteBtn.getToggleState()); };
    soloBtn.onClick = [this] { if (auto* s = proc.getEngine().getSlot (slotIndex)) s->solo.store   (soloBtn.getToggleState()); };
    lockBtn.setColour (TextButton::buttonOnColourId, kYellow);
    muteBtn.setColour (TextButton::buttonOnColourId, kPink);
    soloBtn.setColour (TextButton::buttonOnColourId, kBlue);

    volSlider.setSliderStyle (Slider::LinearHorizontal);
    volSlider.setRange (0.0, 1.0, 0.01);
    volSlider.setValue (0.85, dontSendNotification);
    volSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    volSlider.setColour (Slider::trackColourId, kPink);
    volSlider.onValueChange = [this] { if (auto* s = proc.getEngine().getSlot (slotIndex)) s->gain.store ((float) volSlider.getValue()); };
    addAndMakeVisible (volSlider);
}

void MementoAudioProcessorEditor::SlotRow::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto left = r.removeFromLeft (72);
    roleLabel.setBounds (left.removeFromTop (18));
    auto right = r.removeFromRight (200);
    // boutons à droite
    auto brow = right.removeFromTop (24);
    rerollBtn.setBounds (brow.removeFromLeft (70)); brow.removeFromLeft (4);
    lockBtn.setBounds (brow.removeFromLeft (44)); brow.removeFromLeft (4);
    soloBtn.setBounds (brow.removeFromLeft (26)); brow.removeFromLeft (4);
    muteBtn.setBounds (brow.removeFromLeft (26));
    volSlider.setBounds (right.removeFromTop (20));
    nameLabel.setBounds (r.removeFromTop (20));
    subLabel.setBounds (r.removeFromTop (16));
}

void MementoAudioProcessorEditor::SlotRow::refresh()
{
    auto* s = proc.getEngine().getSlot (slotIndex);
    if (s == nullptr) return;
    roleLabel.setText (mem::roleLabel (s->role), dontSendNotification);
    auto nm = s->displayName + (s->rendering.load() ? "  (rendu…)" : "");
    nameLabel.setText (nm, dontSendNotification);
    subLabel.setText (s->displaySub, dontSendNotification);
    lockBtn.setToggleState (s->locked.load(), dontSendNotification);
    muteBtn.setToggleState (s->mute.load(),   dontSendNotification);
    soloBtn.setToggleState (s->solo.load(),   dontSendNotification);
    if (! volSlider.isMouseButtonDown())
        volSlider.setValue (s->gain.load(), dontSendNotification);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
MementoAudioProcessorEditor::MementoAudioProcessorEditor (MementoAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    titleLabel.setText ("MEMENTO", dontSendNotification);
    titleLabel.setFont (Font (22.0f, Font::bold));
    titleLabel.setColour (Label::textColourId, kInk);
    addAndMakeVisible (titleLabel);

    bpmLabel.setColour (Label::textColourId, kInk);
    bpmLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    counterLabel.setColour (Label::textColourId, Colour (0xff5f5443));
    counterLabel.setFont (Font (12.0f));
    addAndMakeVisible (counterLabel);

    folderLabel.setColour (Label::textColourId, Colour (0xff5f5443));
    folderLabel.setFont (Font (11.0f));
    folderLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (folderLabel);

    folderBtn.onClick = [this] { chooseFolder(); };
    addAndMakeVisible (folderBtn);

    rerollAllBtn.setColour (TextButton::buttonColourId, kPink);
    rerollAllBtn.setColour (TextButton::textColourOffId, Colours::white);
    rerollAllBtn.onClick = [this] { processor.getEngine().rerollAll(); };
    addAndMakeVisible (rerollAllBtn);

    viewport.setViewedComponent (&rowsHolder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setSize (600, 460);
    rebuildRows();
    startTimerHz (8);
}

MementoAudioProcessorEditor::~MementoAudioProcessorEditor() { stopTimer(); }

void MementoAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (kPaper);
    g.setColour (kMint);
    g.fillRect (getLocalBounds().removeFromTop (96));
    g.setColour (kInk);
    g.drawLine (0.0f, 96.0f, (float) getWidth(), 96.0f, 2.0f);
}

void MementoAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (96);
    auto top = header.removeFromTop (44).reduced (12, 8);
    titleLabel.setBounds (top.removeFromLeft (160));
    bpmLabel.setBounds (top.removeFromRight (160));
    auto ctrls = header.reduced (12, 6);
    folderBtn.setBounds (ctrls.removeFromLeft (160).reduced (0, 2));
    rerollAllBtn.setBounds (ctrls.removeFromRight (120).reduced (0, 2));
    counterLabel.setBounds (ctrls.removeFromRight (150));
    folderLabel.setBounds (ctrls);
    viewport.setBounds (r);
    rowsHolder.setSize (viewport.getWidth() - 8, jmax (10, rows.size() * 60));
}

void MementoAudioProcessorEditor::rebuildRows()
{
    rows.clear();
    int n = processor.getEngine().getNumSlots();
    for (int i = 0; i < n; ++i)
    {
        auto* row = new SlotRow (processor, i);
        rowsHolder.addAndMakeVisible (row);
        rows.add (row);
    }
    rowsHolder.setSize (viewport.getWidth() - 8, jmax (10, rows.size() * 60));
    int y = 0;
    for (auto* row : rows) { row->setBounds (0, y, rowsHolder.getWidth(), 58); y += 60; }
    refreshRows();
}

void MementoAudioProcessorEditor::refreshRows()
{
    for (auto* row : rows) row->refresh();

    auto& e = processor.getEngine();
    bpmLabel.setText (String (e.getHostBpm(), 1) + " BPM (hôte)", dontSendNotification);
    counterLabel.setText (String (e.getRecordCount()) + " sons · " + String (e.getUsableCount()) + " loops",
                          dontSendNotification);
    auto f = e.getFolder();
    folderLabel.setText (e.isScanning() ? "Indexation…" : (f.isDirectory() ? f.getFileName() : "Aucun dossier"),
                         dontSendNotification);
}

void MementoAudioProcessorEditor::timerCallback()
{
    if (rows.size() != processor.getEngine().getNumSlots())
    {
        rebuildRows();
        resized();
    }
    refreshRows();
}

void MementoAudioProcessorEditor::chooseFolder()
{
    chooser = std::make_unique<FileChooser> ("Choisir le dossier de samples (ex. Banque Sons)",
                                             File::getSpecialLocation (File::userDesktopDirectory));
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
        [this] (const FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory())
                processor.getEngine().scanFolder (dir);
        });
}

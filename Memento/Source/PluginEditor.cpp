#include "PluginEditor.h"

using namespace juce;

// --- palette ---------------------------------------------------------------
static const Colour kPaper = Colour (0xfff7ecd6);
static const Colour kInk   = Colour (0xff23190f);
static const Colour kMint  = Colour (0xff8fe3cc);
static const Colour kSub   = Colour (0xff6b5d49);
static const Colour kPink  = Colour (0xfff45d9a);
static const Colour kBlue  = Colour (0xff3b82d6);
static const Colour kRed   = Colour (0xffe0453b);
static const Colour kTeal  = Colour (0xff2bb3c0);
static const Colour kGold  = Colour (0xfff2b13a);
static const Colour kCard  = Colour (0xfffffdf8);

static Colour roleColour (mem::Role r)
{
    switch (r)
    {
        case mem::Role::Drums:   return Colour (0xfff45d9a);
        case mem::Role::Perc:    return Colour (0xfff0863c);
        case mem::Role::Bass:    return Colour (0xff3b82d6);
        case mem::Role::Tonal:   return Colour (0xff7b61ff);
        case mem::Role::Texture: return Colour (0xff2fb79a);
        case mem::Role::Vox:     return Colour (0xfff2b13a);
        case mem::Role::Fx:      return Colour (0xff2bb3c0);
        case mem::Role::Roll:    return Colour (0xffe0453b);
        default:                 return Colour (0xff8a7a63);
    }
}

// ===========================================================================
// StemControl — clic = export dossier, glisser = drag & drop vers le DAW
// ===========================================================================
void MementoAudioProcessorEditor::StemControl::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (kInk);
    g.fillRoundedRectangle (b, 6.0f);

    auto top = b.removeFromTop (b.getHeight() * 0.60f);
    g.setColour (kPaper);
    g.setFont (Font (11.0f, Font::bold));
    g.drawText ("STEMS", top, Justification::centred);

    g.setColour (kMint);
    g.setFont (Font (9.0f));
    g.drawText (String::fromUTF8 ("glisser \xE2\x86\x92 DAW"), b, Justification::centred);
}

void MementoAudioProcessorEditor::StemControl::mouseDown (const MouseEvent&)
{
    dragging = false;
}

void MementoAudioProcessorEditor::StemControl::mouseDrag (const MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 6)
    {
        dragging = true;
        if (owner != nullptr) owner->startStemDrag (slotIndex);
    }
}

void MementoAudioProcessorEditor::StemControl::mouseUp (const MouseEvent&)
{
    if (! dragging && owner != nullptr) owner->exportSlotStem (slotIndex);
    dragging = false;
}

// ===========================================================================
// SlotRow
// ===========================================================================
MementoAudioProcessorEditor::SlotRow::SlotRow (MementoAudioProcessor& p,
                                               MementoAudioProcessorEditor& o, int index)
    : proc (p), owner (o), slotIndex (index)
{
    roleLabel.setFont (Font (12.0f, Font::bold));
    roleLabel.setJustificationType (Justification::centred);
    roleLabel.setColour (Label::textColourId, Colours::white);
    addAndMakeVisible (roleLabel);

    nameLabel.setFont (Font (15.0f, Font::bold));
    nameLabel.setColour (Label::textColourId, kInk);
    nameLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (nameLabel);

    subLabel.setFont (Font (11.5f));
    subLabel.setColour (Label::textColourId, kSub);
    subLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (subLabel);

    rerollBtn.onClick = [this] { proc.getEngine().rerollSlot (slotIndex); };
    addAndMakeVisible (rerollBtn);

    for (auto* b : { &lockBtn, &muteBtn, &soloBtn }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
    lockBtn.onClick = [this] { if (auto* s = proc.getEngine().getSlot (slotIndex)) s->locked.store (lockBtn.getToggleState()); };
    muteBtn.onClick = [this] { if (auto* s = proc.getEngine().getSlot (slotIndex)) s->mute.store   (muteBtn.getToggleState()); };
    soloBtn.onClick = [this] { if (auto* s = proc.getEngine().getSlot (slotIndex)) s->solo.store   (soloBtn.getToggleState()); };
    lockBtn.setColour (TextButton::buttonOnColourId, Colour (0xfff5cf4b));
    muteBtn.setColour (TextButton::buttonOnColourId, kPink);
    soloBtn.setColour (TextButton::buttonOnColourId, kBlue);

    // bouton STEMS (clic + drag)
    stems.owner = &owner;
    stems.slotIndex = slotIndex;
    addAndMakeVisible (stems);

    // combo d'accordage par slot
    tuneBox.addItem ("Auto", 1);
    tuneBox.addItem ("Original", 2);
    for (int s = -12; s <= 12; ++s)
    {
        String lab = (s > 0 ? "+" : "") + String (s) + " st";
        tuneBox.addItem (lab, 112 + s);   // id = 112 + semitones (100..124)
    }
    tuneBox.setColour (ComboBox::backgroundColourId, kPaper);
    tuneBox.setTextWhenNothingSelected ("Tune");
    tuneBox.onChange = [this]
    {
        const int id = tuneBox.getSelectedId();
        auto& e = proc.getEngine();
        if      (id == 1) e.setSlotTuneMode  (slotIndex, mem::TuneMode::Auto);
        else if (id == 2) e.setSlotTuneMode  (slotIndex, mem::TuneMode::Original);
        else if (id >= 100) e.setSlotManualSemis (slotIndex, id - 112);
    };
    addAndMakeVisible (tuneBox);

    removeBtn.setColour (TextButton::buttonColourId, Colour (0x22000000));
    removeBtn.setColour (TextButton::textColourOffId, kInk);
    removeBtn.onClick = [this] { owner.requestRemoveSlot (slotIndex); };
    addAndMakeVisible (removeBtn);

    volSlider.setSliderStyle (Slider::LinearHorizontal);
    volSlider.setRange (0.0, 1.0, 0.01);
    volSlider.setValue (0.85, dontSendNotification);
    volSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    volSlider.onValueChange = [this] { if (auto* s = proc.getEngine().getSlot (slotIndex)) s->gain.store ((float) volSlider.getValue()); };
    addAndMakeVisible (volSlider);
}

void MementoAudioProcessorEditor::SlotRow::paint (Graphics& g)
{
    auto b = getLocalBounds().reduced (4, 4).toFloat();
    g.setColour (kCard);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (Colour (0x1a000000));
    g.drawRoundedRectangle (b, 10.0f, 1.0f);

    auto chipR = b.removeFromLeft (66).reduced (8, 12);
    g.setColour (chip);
    g.fillRoundedRectangle (chipR, 7.0f);
}

void MementoAudioProcessorEditor::SlotRow::resized()
{
    auto r = getLocalBounds().reduced (4, 4);
    auto chipR = r.removeFromLeft (66).reduced (8, 12);
    roleLabel.setBounds (chipR);
    r.removeFromLeft (8);

    auto right = r.removeFromRight (272);
    auto rTop = right.removeFromTop (right.getHeight() / 2).reduced (2, 5);
    auto rBot = right.reduced (2, 5);
    rerollBtn.setBounds (rTop.removeFromLeft (84)); rTop.removeFromLeft (5);
    lockBtn.setBounds   (rTop.removeFromLeft (56)); rTop.removeFromLeft (5);
    soloBtn.setBounds   (rTop.removeFromLeft (26)); rTop.removeFromLeft (4);
    muteBtn.setBounds   (rTop.removeFromLeft (26));

    stems.setBounds     (rBot.removeFromLeft (78)); rBot.removeFromLeft (6);
    removeBtn.setBounds  (rBot.removeFromRight (28)); rBot.removeFromRight (6);
    tuneBox.setBounds    (rBot);

    r.removeFromRight (8);
    nameLabel.setBounds (r.removeFromTop (22));
    subLabel.setBounds  (r.removeFromTop (16));
    r.removeFromTop (2);
    volSlider.setBounds (r.removeFromTop (20));
}

void MementoAudioProcessorEditor::SlotRow::refresh()
{
    auto* s = proc.getEngine().getSlot (slotIndex);
    if (s == nullptr) return;
    chip = roleColour (s->role);
    roleLabel.setText (mem::roleLabel (s->role), dontSendNotification);
    auto nm = s->displayName + (s->rendering.load() ? juce::String::fromUTF8 ("  (rendu\xE2\x80\xA6)") : String());
    nameLabel.setText (nm, dontSendNotification);

    // sous-titre : pack · BPM · SAMPLE KEY  (+ transpose appliqué)
    String sub = s->displaySub;
    const int semis = s->semis.load();
    if (s->tonal.load() && semis != 0)
        sub += juce::String::fromUTF8 (" \xC2\xB7 ") + (semis > 0 ? "+" : "") + String (semis) + "st";
    subLabel.setText (sub, dontSendNotification);

    lockBtn.setToggleState (s->locked.load(), dontSendNotification);
    muteBtn.setToggleState (s->mute.load(),   dontSendNotification);
    soloBtn.setToggleState (s->solo.load(),   dontSendNotification);
    if (! volSlider.isMouseButtonDown())
        volSlider.setValue (s->gain.load(), dontSendNotification);

    // accordage : reflète l'état moteur, désactivé si non-tonal
    const bool tonal = s->tonal.load();
    tuneBox.setEnabled (tonal);
    const auto m = proc.getEngine().getSlotTuneMode (slotIndex);
    int id = 1;
    if      (m == mem::TuneMode::Original) id = 2;
    else if (m == mem::TuneMode::Manual)   id = 112 + jlimit (-12, 12, semis);
    else                                    id = 1; // Auto
    if (tuneBox.getSelectedId() != id)
        tuneBox.setSelectedId (id, dontSendNotification);
}

// ===========================================================================
// Editor
// ===========================================================================
MementoAudioProcessorEditor::MementoAudioProcessorEditor (MementoAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lnf.setColour (ResizableWindow::backgroundColourId, kPaper);
    lnf.setColour (TextButton::buttonColourId, Colour (0xff2a2018));
    lnf.setColour (TextButton::textColourOffId, kPaper);
    lnf.setColour (TextButton::textColourOnId, Colours::white);
    lnf.setColour (ComboBox::backgroundColourId, kCard);
    lnf.setColour (ComboBox::textColourId, kInk);
    lnf.setColour (ComboBox::arrowColourId, kInk);
    lnf.setColour (ComboBox::outlineColourId, Colour (0x33000000));
    lnf.setColour (PopupMenu::backgroundColourId, kCard);
    lnf.setColour (PopupMenu::textColourId, kInk);
    lnf.setColour (PopupMenu::highlightedBackgroundColourId, kMint);
    lnf.setColour (PopupMenu::highlightedTextColourId, kInk);
    lnf.setColour (Slider::backgroundColourId, Colour (0x22000000));
    lnf.setColour (Slider::trackColourId, kPink);
    lnf.setColour (Slider::thumbColourId, kInk);
    setLookAndFeel (&lnf);

    titleLabel.setText ("MEMENTO", dontSendNotification);
    titleLabel.setFont (Font (26.0f, Font::bold));
    titleLabel.setColour (Label::textColourId, kInk);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText (juce::String::fromUTF8 ("song starter \xC2\xB7 AU \xC2\xB7 v0.3.2"), dontSendNotification);
    subtitleLabel.setFont (Font (12.5f));
    subtitleLabel.setColour (Label::textColourId, kSub);
    subtitleLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    bpmLabel.setColour (Label::textColourId, kInk);
    bpmLabel.setFont (Font (14.0f, Font::bold));
    bpmLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    counterLabel.setColour (Label::textColourId, kSub);
    counterLabel.setFont (Font (12.0f));
    addAndMakeVisible (counterLabel);

    folderLabel.setColour (Label::textColourId, kSub);
    folderLabel.setFont (Font (11.5f));
    folderLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (folderLabel);

    statusLabel.setColour (Label::textColourId, kBlue);
    statusLabel.setFont (Font (11.5f, Font::bold));
    statusLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (statusLabel);

    auto accent = [] (TextButton& b, Colour c) {
        b.setColour (TextButton::buttonColourId, c);
        b.setColour (TextButton::textColourOffId, Colours::white);
    };
    accent (rerollAllBtn, kPink);
    accent (exportAllBtn, kBlue);
    accent (genRollBtn,   kRed);
    accent (loopStyleBtn, kTeal);

    folderBtn.onClick    = [this] { chooseFolder(); };
    stylesBtn.onClick    = [this] { chooseStylesFolder(); };
    rerollAllBtn.onClick = [this] { processor.getEngine().rerollAll(); };
    exportAllBtn.onClick = [this] { exportAllStems(); };
    genRollBtn.onClick   = [this] { generateRoll(); };
    loopStyleBtn.onClick = [this] { addStyleLoop(); };
    for (auto* b : { &folderBtn, &stylesBtn, &rerollAllBtn, &exportAllBtn, &genRollBtn, &loopStyleBtn })
        addAndMakeVisible (*b);

    styleCombo.setTextWhenNothingSelected (juce::String::fromUTF8 ("(aucun style)"));
    addAndMakeVisible (styleCombo);

    // --- accordage global ---
    keyLabel.setText ("PROJECT KEY", dontSendNotification);
    keyLabel.setColour (Label::textColourId, kInk);
    keyLabel.setFont (Font (12.0f, Font::bold));
    keyLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (keyLabel);

    buildProjectKeyBox();
    projectKeyBox.onChange = [this]
    {
        const int id = projectKeyBox.getSelectedId();
        mem::Key k;
        if (id >= 100)
        {
            k.root = (id - 100) / 2;
            k.mode = ((id - 100) % 2 == 0) ? mem::Mode::Major : mem::Mode::Minor;
        }
        processor.getEngine().setProjectKey (k);
        setStatus (juce::String::fromUTF8 ("Tonalit\xC3\xA9 projet : ")
                   + (k.hasRoot() ? mem::KeyParser::toString (k) : juce::String::fromUTF8 ("\xE2\x80\x94")));
    };
    addAndMakeVisible (projectKeyBox);

    keySyncBtn.setClickingTogglesState (true);
    keySyncBtn.setColour (TextButton::buttonColourId, Colour (0xff2a2018));
    keySyncBtn.setColour (TextButton::buttonOnColourId, kMint);
    keySyncBtn.setColour (TextButton::textColourOnId, kInk);
    keySyncBtn.onClick = [this]
    {
        processor.getEngine().setKeySync (keySyncBtn.getToggleState());
        setStatus (keySyncBtn.getToggleState() ? juce::String::fromUTF8 ("KEY SYNC activ\xC3\xA9")
                                                : juce::String::fromUTF8 ("KEY SYNC d\xC3\xA9sactiv\xC3\xA9"));
    };
    addAndMakeVisible (keySyncBtn);

    viewport.setViewedComponent (&rowsHolder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setSize (720, 624);
    rebuildRows();
    refreshStyles();
    startTimerHz (8);
}

MementoAudioProcessorEditor::~MementoAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MementoAudioProcessorEditor::buildProjectKeyBox()
{
    projectKeyBox.clear (dontSendNotification);
    projectKeyBox.addItem (juce::String::fromUTF8 ("\xE2\x80\x94 (aucune)"), 1);
    for (int r = 0; r < 12; ++r)
    {
        projectKeyBox.addItem (mem::KeyParser::rootName (r) + " Major", 100 + r * 2);
        projectKeyBox.addItem (mem::KeyParser::rootName (r) + " Minor", 101 + r * 2);
    }
    projectKeyBox.setSelectedId (1, dontSendNotification);
}

void MementoAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (kPaper);
    auto band = getLocalBounds().removeFromTop (62);
    g.setColour (kMint);
    g.fillRect (band);
    g.setColour (kInk);
    g.fillRect (band.removeFromBottom (2));
}

void MementoAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    auto titleBand = r.removeFromTop (62).reduced (16, 10);
    bpmLabel.setBounds (titleBand.removeFromRight (150));
    titleLabel.setBounds (titleBand.removeFromLeft (190));
    subtitleLabel.setBounds (titleBand);

    auto rowA = r.removeFromTop (36).reduced (16, 4);
    folderBtn.setBounds (rowA.removeFromLeft (150)); rowA.removeFromLeft (8);
    rerollAllBtn.setBounds (rowA.removeFromLeft (110));
    exportAllBtn.setBounds (rowA.removeFromRight (170));

    auto rowB = r.removeFromTop (36).reduced (16, 4);
    stylesBtn.setBounds (rowB.removeFromLeft (110)); rowB.removeFromLeft (8);
    styleCombo.setBounds (rowB.removeFromLeft (180)); rowB.removeFromLeft (8);
    genRollBtn.setBounds (rowB.removeFromLeft (150)); rowB.removeFromLeft (8);
    loopStyleBtn.setBounds (rowB.removeFromLeft (140));

    auto rowC = r.removeFromTop (34).reduced (16, 3);
    keyLabel.setBounds (rowC.removeFromLeft (96)); rowC.removeFromLeft (4);
    projectKeyBox.setBounds (rowC.removeFromLeft (150)); rowC.removeFromLeft (10);
    keySyncBtn.setBounds (rowC.removeFromLeft (120));

    auto info = r.removeFromTop (24).reduced (16, 2);
    statusLabel.setBounds (info.removeFromRight (240));
    counterLabel.setBounds (info.removeFromLeft (180));
    folderLabel.setBounds (info);

    viewport.setBounds (r.reduced (8, 4));
    rowsHolder.setSize (viewport.getWidth() - 8, jmax (10, rows.size() * 80));
    int y = 0;
    for (auto* row : rows) { row->setBounds (0, y, rowsHolder.getWidth(), 78); y += 80; }
}

void MementoAudioProcessorEditor::rebuildRows()
{
    rows.clear();
    int n = processor.getEngine().getNumSlots();
    for (int i = 0; i < n; ++i)
    {
        auto* row = new SlotRow (processor, *this, i);
        rowsHolder.addAndMakeVisible (row);
        rows.add (row);
    }
    rowsHolder.setSize (viewport.getWidth() - 8, jmax (10, rows.size() * 80));
    int y = 0;
    for (auto* row : rows) { row->setBounds (0, y, rowsHolder.getWidth(), 78); y += 80; }
    refreshRows();
}

void MementoAudioProcessorEditor::refreshRows()
{
    for (auto* row : rows) row->refresh();

    auto& e = processor.getEngine();
    bpmLabel.setText (String (e.getHostBpm(), 1) + juce::String::fromUTF8 (" BPM \xE2\x80\xA2 h\xC3\xB4te"), dontSendNotification);
    counterLabel.setText (String (e.getRecordCount()) + " sons / " + String (e.getUsableCount()) + " loops",
                          dontSendNotification);
    auto f = e.getFolder();
    folderLabel.setText (e.isScanning() ? juce::String::fromUTF8 ("Indexation\xE2\x80\xA6")
                                        : (f.isDirectory() ? f.getFileName() : String ("Aucun dossier")),
                         dontSendNotification);

    // reflète l'état d'accordage global
    auto pk = e.getProjectKey();
    int pid = 1;
    if (pk.hasRoot()) pid = 100 + pk.root * 2 + (pk.mode == mem::Mode::Minor ? 1 : 0);
    if (projectKeyBox.getSelectedId() != pid)
        projectKeyBox.setSelectedId (pid, dontSendNotification);
    keySyncBtn.setToggleState (e.getKeySync(), dontSendNotification);
}

void MementoAudioProcessorEditor::refreshStyles()
{
    auto names = processor.getEngine().getStyleNames();
    String sig = names.joinIntoString ("|");
    if (sig == lastStyleSig) return;
    lastStyleSig = sig;

    String prev = styleCombo.getText();
    styleCombo.clear (dontSendNotification);
    if (! names.isEmpty())
    {
        styleCombo.addItemList (names, 1);
        int idx = names.indexOf (prev);
        styleCombo.setSelectedItemIndex (idx >= 0 ? idx : 0, dontSendNotification);
    }
}

juce::String MementoAudioProcessorEditor::currentStyle() const
{
    return styleCombo.getSelectedId() > 0 ? styleCombo.getText() : String();
}

void MementoAudioProcessorEditor::timerCallback()
{
    if (rows.size() != processor.getEngine().getNumSlots())
    {
        rebuildRows();
        resized();
    }
    refreshRows();
    refreshStyles();

    // statut de scan (Scanning… / Library ready — N samples)
    auto& e = processor.getEngine();
    if (statusCountdown == 0)
        statusLabel.setText (e.getStatusText(), dontSendNotification);

    if (statusCountdown > 0 && --statusCountdown == 0)
        statusLabel.setText (e.getStatusText(), dontSendNotification);
}

void MementoAudioProcessorEditor::setStatus (const String& text, int ticks)
{
    statusLabel.setText (text, dontSendNotification);
    statusCountdown = ticks;
}

void MementoAudioProcessorEditor::chooseFolder()
{
    chooser = std::make_unique<FileChooser> (juce::String::fromUTF8 ("Choisir le dossier de samples (ex. Banque Sons)"),
                                             File::getSpecialLocation (File::userDesktopDirectory));
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
        [this] (const FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory()) processor.getEngine().scanFolder (dir);
        });
}

void MementoAudioProcessorEditor::chooseStylesFolder()
{
    chooser = std::make_unique<FileChooser> (juce::String::fromUTF8 ("Choisir le dossier racine des styles (contient tech-house/, house/\xE2\x80\xA6)"),
                                             File::getSpecialLocation (File::userDesktopDirectory));
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
        [this] (const FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                processor.getEngine().setStylesFolder (dir);
                setStatus (juce::String::fromUTF8 ("Styles : ") + dir.getFileName());
            }
        });
}

void MementoAudioProcessorEditor::exportAllStems()
{
    chooser = std::make_unique<FileChooser> (juce::String::fromUTF8 ("Dossier de destination des stems (WAV 32-bit float)"),
                                             File::getSpecialLocation (File::userDesktopDirectory));
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
        [this] (const FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (! dir.isDirectory()) return;
            int n = processor.getEngine().exportAllStems (dir);
            setStatus (String (n) + juce::String::fromUTF8 (" stems export\xC3\xA9s \xE2\x86\x92 ") + dir.getFileName());
        });
}

void MementoAudioProcessorEditor::exportSlotStem (int slotIndex)
{
    chooser = std::make_unique<FileChooser> (juce::String::fromUTF8 ("Dossier de destination du stem (WAV 32-bit float)"),
                                             File::getSpecialLocation (File::userDesktopDirectory));
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
        [this, slotIndex] (const FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (! dir.isDirectory()) return;
            bool ok = processor.getEngine().exportStem (slotIndex, dir);
            setStatus (ok ? juce::String::fromUTF8 ("Stem export\xC3\xA9 \xE2\x86\x92 ") + dir.getFileName()
                          : juce::String::fromUTF8 ("\xC3\x89" "chec de l'export (slot vide ?)"));
        });
}

void MementoAudioProcessorEditor::startStemDrag (int slotIndex)
{
    auto f = processor.getEngine().writeStemToTemp (slotIndex);
    if (f.existsAsFile())
        performExternalDragDropOfFiles (StringArray (f.getFullPathName()), false, this);
    else
        setStatus (juce::String::fromUTF8 ("Rien \xC3\xA0 glisser (slot vide ?)"));
}

void MementoAudioProcessorEditor::generateRoll()
{
    auto style = currentStyle();
    if (style.isEmpty()) { setStatus (juce::String::fromUTF8 ("Choisis d'abord un style")); return; }
    bool ok = processor.getEngine().generateStyleArrangement (style);
    setStatus (ok ? juce::String::fromUTF8 ("Style g\xC3\xA9n\xC3\xA9r\xC3\xA9 sur toutes les pistes : ") + style
                  : juce::String::fromUTF8 ("Style vide ou introuvable"));
}

void MementoAudioProcessorEditor::addStyleLoop()
{
    auto style = currentStyle();
    if (style.isEmpty()) { setStatus (juce::String::fromUTF8 ("Choisis d'abord un style")); return; }
    bool ok = processor.getEngine().addStyleLoopSlot (style);
    setStatus (ok ? juce::String::fromUTF8 ("Boucle de style : ") + style
                  : juce::String::fromUTF8 ("Maximum 4 pistes \xE2\x80\x94 d\xC3\xA9verrouille ou supprime une piste"));
}

void MementoAudioProcessorEditor::requestRemoveSlot (int slotIndex)
{
    processor.getEngine().removeSlot (slotIndex);
}

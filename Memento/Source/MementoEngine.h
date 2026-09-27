#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

// ===========================================================================
// Memento — moteur d'assemblage (v0.2).
//   • SampleLibrary : scan récursif d'un dossier, parsing BPM + rôle depuis le
//     nom de fichier (repris de l'app Electron Memento).
//   • Assembler     : slots par rôle, reroll/lock, rendu offline calé au tempo
//     de l'hôte, et mixage multi-voix synchronisé dans process().
//   • Styles        : découverte dynamique de sous-dossiers /styles/<nom>/.
//   • Rolls         : slots rythmiques (retrigger sur la grille + variations)
//                     et slots "boucle de style" (pioche calée au tempo).
//   • Export stems  : un WAV 32-bit float par slot, longueur calée sur les
//                     mesures, prêt à réimporter dans un DAW.
// Le rendu (chargement + calage) tourne sur un thread de fond ; le thread
// audio ne fait que lire des buffers déjà prêts (échange atomique de pointeur).
// ===========================================================================

namespace mem
{

enum class Role { Drums, Perc, Bass, Tonal, Texture, Vox, Fx, Roll, NumRoles };

inline const char* roleLabel (Role r)
{
    switch (r) {
        case Role::Drums:   return "Drums";
        case Role::Perc:    return "Perc";
        case Role::Bass:    return "Bass";
        case Role::Tonal:   return "Tonal";
        case Role::Texture: return "Texture";
        case Role::Vox:     return "Vox";
        case Role::Fx:      return "FX";
        case Role::Roll:    return "Roll";
        default:            return "?";
    }
}

// D'où provient le contenu d'un slot.
enum class SlotSource { Library, StyleLoop, Roll };

// Un enregistrement de la bibliothèque.
struct Record
{
    juce::File   file;
    juce::String name;     // sans extension
    juce::String pack;     // dossier parent
    Role         role  = Role::Tonal;
    int          bpm   = 0;     // 0 = inconnu
    bool         isLoop = false;
};

// Un clip rendu : audio au SR de l'hôte, prêt à être bouclé.
struct StretchedClip
{
    juce::AudioBuffer<float> buffer;      // stéréo, SR hôte
    juce::int64              loopLen = 0; // échantillons, calé sur des mesures entières
    juce::String             name;
    int                      sourceBpm = 0;
};

// Un style découvert dans le dossier /styles/.
struct StyleInfo
{
    juce::String       name;   // = nom du sous-dossier
    juce::File         dir;
    juce::Array<juce::File> files;   // fichiers audio du style
};

// Un slot d'assemblage.
struct Slot
{
    Role       role   = Role::Drums;
    SlotSource source = SlotSource::Library;

    int          recordIndex = -1;   // source Library : index dans records
    juce::File   fileSource;         // source StyleLoop / Roll : fichier choisi
    juce::String styleName;          // source StyleLoop / Roll : style courant
    int          rollSeed = 0;       // source Roll : graine de variation

    // clip courant — accédé en atomique (std::atomic_load/store sur shared_ptr)
    std::shared_ptr<StretchedClip> clip;

    std::atomic<float> gain  { 0.85f };
    std::atomic<float> pan   { 0.0f };
    std::atomic<bool>  mute   { false };
    std::atomic<bool>  solo   { false };
    std::atomic<bool>  locked { false };
    std::atomic<bool>  rendering { false };

    juce::String displayName { "—" };
    juce::String displaySub  { "—" };
};

// ---------------------------------------------------------------------------

class MementoEngine : private juce::Thread
{
public:
    MementoEngine();
    ~MementoEngine() override;

    // --- configuration audio ---
    void prepare (double sampleRate);

    // --- bibliothèque ---
    void scanFolder (const juce::File& folder);      // (thread appelant : message thread)
    juce::File getFolder() const { return libraryFolder; }
    int  getRecordCount() const;
    int  getUsableCount() const;
    bool isScanning() const { return scanning.load(); }

    // --- styles ---
    void setStylesFolder (const juce::File& folder);   // bouton STYLES
    juce::File getStylesFolder() const { return stylesFolder; }
    juce::StringArray getStyleNames() const;           // découverte dynamique

    // --- tempo hôte ---
    void setHostBpm (double bpm);
    double getHostBpm() const { return hostBpm.load(); }

    // --- slots ---
    int  getNumSlots() const { return (int) slots.size(); }
    Slot* getSlot (int i) { return (i >= 0 && i < (int) slots.size()) ? slots[(size_t) i].get() : nullptr; }
    void  setDefaultSlots();                 // Drums/Bass/Tonal/Texture/Vox
    void  addSlot (Role role);
    void  addStyleLoopSlot (const juce::String& style);  // pioche une boucle du style
    void  addRollSlot (const juce::String& style);       // roll rythmique généré
    void  removeSlot (int index);

    void  rerollSlot (int index);            // (re)génère selon la source du slot
    void  rerollAll();                       // slots non verrouillés

    // --- export stems (thread message) ---
    // Écrit un WAV 32-bit float par slot ; longueur = boucle calée sur les
    // mesures. Renvoie true / le nombre de fichiers écrits.
    bool exportStem     (int slotIndex, const juce::File& destDir);
    int  exportAllStems (const juce::File& destDir);

    // --- lecture (thread audio) ---
    void  process (juce::AudioBuffer<float>& out, juce::int64 hostPosSamples, bool playing);

    // --- persistance ---
    juce::String saveStateString() const;
    void         loadStateString (const juce::String& s);

    std::function<void()> onLibraryChanged;   // notif UI (thread message)
    std::function<void()> onSlotsChanged;     // notif UI
    std::function<void()> onStylesChanged;    // notif UI

private:
    // Thread de rendu.
    void run() override;
    void doScan (const juce::File& folder);
    void doStyleScan();
    void requestRender (int slotIndex);
    void requestRenderAll();
    void renderSlot (int slotIndex);

    // Sélection.
    std::vector<int> candidatesForRole (Role role) const;
    int  pickForRole (Role role, int avoidIndex) const;

    // Styles (thread message : lecture cache).
    juce::Array<juce::File> filesForStyle (const juce::String& style) const;

    // Rendu offline.
    static Record recordFromFile (const juce::File& f);
    std::shared_ptr<StretchedClip> makeClip     (const Record& rec);   // boucle calée au tempo
    std::shared_ptr<StretchedClip> makeRollClip (const juce::File& src, int seed);  // roll rythmique
    std::shared_ptr<StretchedClip> loadResampled (const juce::File& f, double maxSeconds, int& outFileCh);

    bool writeClipToWav (const StretchedClip& clip, const juce::File& outFile) const;

    // --- données ---
    juce::File              libraryFolder;
    std::vector<Record>     records;                 // stable après scan
    std::vector<std::unique_ptr<Slot>> slots;
    std::atomic<double>     hostBpm { 124.0 };
    double                  sr = 44100.0;
    std::atomic<bool>       scanning { false };

    juce::File              stylesFolder;            // racine explicite (bouton STYLES)
    std::vector<StyleInfo>  styleList;               // cache (verrou stylesLock)

    juce::AudioFormatManager formatManager;
    mutable juce::Random    rng;

    // Verrous : données (records), structure des slots, styles.
    mutable juce::CriticalSection dataLock;
    mutable juce::CriticalSection slotsLock;
    mutable juce::CriticalSection stylesLock;

    // File de rendu (indices de slots à (re)rendre) + scans en attente.
    juce::CriticalSection   jobLock;
    std::vector<int>        jobQueue;
    juce::File              pendingScan;
    std::atomic<bool>       pendingStyleRescan { false };
    juce::WaitableEvent     jobEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MementoEngine)
};

} // namespace mem

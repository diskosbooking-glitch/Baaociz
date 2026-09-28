#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

// ===========================================================================
// Memento — moteur d'assemblage (v0.3).
//   • SampleLibrary : scan récursif d'un dossier racine (= bibliothèque), index
//     incrémental (mtime/taille), parsing BPM + rôle + tonalité depuis le nom.
//   • Styles        : découverte dynamique des sous-dossiers de la racine.
//   • Assembler     : slots par rôle, reroll/lock, rendu offline calé au tempo,
//     accordage (pitch-shift granulaire séparé du time-stretch), mixage.
//   • Rolls         : slots rythmiques (retrigger sur la grille + variations)
//     et slots "boucle de style".
//   • Export stems  : WAV 32-bit float, longueur commune calée sur les mesures
//     (même départ / même durée) — dossier ou fichier temporaire (drag & drop).
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

// Un rôle est-il tonal (candidat à l'accordage) ?
inline bool roleIsTonal (Role r)
{
    return r == Role::Bass || r == Role::Tonal || r == Role::Vox;
}

enum class SlotSource { Library, StyleLoop, Roll };

// --- Tonalité -----------------------------------------------------------------
enum class Mode { Unknown, Major, Minor };

// root : 0..11 (C=0, C#=1, … B=11) ; -1 = inconnu.
struct Key
{
    int  root = -1;
    Mode mode = Mode::Unknown;

    bool hasRoot() const { return root >= 0; }
    bool isFull()  const { return root >= 0 && mode != Mode::Unknown; }
};

// Analyse de tonalité depuis un nom de fichier (+ Camelot). Statique/pur.
struct KeyParser
{
    static Key    fromName (const juce::String& name);   // "Am", "A Minor", "F#", "8A"…
    static juce::String toString (const Key& k);         // "A Minor", "F# (root)", "—"
    static int    rootFromToken (const juce::String& tok); // "F#","Db"→ 0..11 ou -1
    static juce::String rootName (int root, bool preferFlat = false);
};

enum class TuneMode { Auto, Manual, Original };

// Un enregistrement de la bibliothèque (indexé).
struct Record
{
    juce::File   file;
    juce::String name;     // sans extension
    juce::String pack;     // dossier parent
    Role         role  = Role::Tonal;
    int          bpm   = 0;     // 0 = inconnu
    bool         isLoop = false;
    Key          key;          // tonalité détectée
    juce::int64  mtime = 0;     // pour l'index incrémental
    juce::int64  size  = 0;
};

// Un clip rendu : audio au SR de l'hôte, prêt à être bouclé.
struct StretchedClip
{
    juce::AudioBuffer<float> buffer;      // stéréo, SR hôte
    juce::int64              loopLen = 0; // échantillons, calé sur des mesures entières
    juce::String             name;
    int                      sourceBpm = 0;
};

struct StyleInfo
{
    juce::String       name;
    juce::File         dir;
    juce::Array<juce::File> files;
};

// Un slot d'assemblage.
struct Slot
{
    Role       role   = Role::Drums;
    SlotSource source = SlotSource::Library;

    int          recordIndex = -1;
    juce::File   fileSource;
    juce::String styleName;
    int          rollSeed = 0;

    std::shared_ptr<StretchedClip> clip;

    std::atomic<float> gain  { 0.85f };
    std::atomic<float> pan   { 0.0f };
    std::atomic<bool>  mute   { false };
    std::atomic<bool>  solo   { false };
    std::atomic<bool>  locked { false };
    std::atomic<bool>  rendering { false };

    // --- accordage ---
    Key                 sampleKey;                 // tonalité du sample courant
    Key                 manualTarget;              // cible en mode Manual (optionnel)
    std::atomic<int>    tuneMode  { (int) TuneMode::Auto };
    std::atomic<int>    semis     { 0 };            // demi-tons appliqués (rendu)
    std::atomic<int>    cents     { 0 };            // cents fins
    std::atomic<bool>   tonal     { false };        // dérivé du rôle

    juce::String displayName { "—" };
    juce::String displaySub  { "—" };
};

// ---------------------------------------------------------------------------

class MementoEngine : private juce::Thread
{
public:
    MementoEngine();
    ~MementoEngine() override;

    void prepare (double sampleRate);

    // --- bibliothèque (racine) ---
    void scanFolder (const juce::File& folder);
    juce::File getFolder() const { return libraryFolder; }
    int  getRecordCount() const;
    int  getUsableCount() const;
    bool isScanning() const { return scanning.load(); }
    juce::String getStatusText() const;             // "Scanning…" / "Library ready — N samples"

    // --- styles (sous-dossiers de la racine) ---
    void setStylesFolder (const juce::File& folder);
    juce::File getStylesFolder() const { return stylesFolder; }
    juce::StringArray getStyleNames() const;

    // --- tempo hôte ---
    void setHostBpm (double bpm);
    double getHostBpm() const { return hostBpm.load(); }

    // --- accordage global ---
    void setProjectKey (const Key& k);              // "PROJECT KEY" (cible Auto)
    Key  getProjectKey() const;
    void setKeySync (bool on);                      // KEY SYNC global
    bool getKeySync() const { return keySync.load(); }

    // par slot
    void setSlotTuneMode (int slotIndex, TuneMode m);
    void setSlotManualSemis (int slotIndex, int s); // mode Manual : demi-tons directs
    TuneMode getSlotTuneMode (int slotIndex) const;

    // --- slots ---
    static constexpr int kMaxSlots = 4;             // nombre de pistes maximum
    int  getNumSlots() const { return (int) slots.size(); }
    Slot* getSlot (int i) { return (i >= 0 && i < (int) slots.size()) ? slots[(size_t) i].get() : nullptr; }
    void  setDefaultSlots();
    void  addSlot (Role role);
    bool  addStyleLoopSlot (const juce::String& style);   // false si 4 pistes verrouillées
    bool  addRollSlot (const juce::String& style);        // false si 4 pistes verrouillées
    void  removeSlot (int index);

    void  rerollSlot (int index);
    void  rerollAll();

    // --- export stems (thread message) ---
    // Longueur commune calée sur les mesures (tous les stems même départ/durée).
    bool exportStem     (int slotIndex, const juce::File& destDir);
    int  exportAllStems (const juce::File& destDir);
    juce::File writeStemToTemp (int slotIndex);     // pour le drag & drop DAW

    // --- lecture (thread audio) ---
    void  process (juce::AudioBuffer<float>& out, juce::int64 hostPosSamples, bool playing);

    // --- persistance ---
    juce::String saveStateString() const;
    void         loadStateString (const juce::String& s);

    std::function<void()> onLibraryChanged;
    std::function<void()> onSlotsChanged;
    std::function<void()> onStylesChanged;

private:
    void run() override;
    void doScan (const juce::File& folder);
    void doStyleScan();
    void requestRender (int slotIndex);
    void requestRenderAll();
    void renderSlot (int slotIndex);

    std::vector<int> candidatesForRole (Role role) const;
    int  pickForRole (Role role, int avoidIndex) const;

    // Renvoie l'index d'un slot pour une nouvelle source : ajoute si < kMaxSlots,
    // sinon réutilise le dernier slot non-verrouillé ; -1 si tous verrouillés.
    // (slotsLock doit être détenu par l'appelant.)
    int  slotForNewSource();

    juce::Array<juce::File> filesForStyle (const juce::String& style) const;

    // accordage : calcule les demi-tons effectifs d'un slot.
    void recomputeSlotTuning (int slotIndex);
    void recomputeAllTuning();
    static int transposeSemitones (const Key& from, const Key& to); // minimisé [-6..+6]

    // rendu offline.
    static Record recordFromFile (const juce::File& f);
    std::shared_ptr<StretchedClip> makeClip     (const Record& rec);
    std::shared_ptr<StretchedClip> makeRollClip (const juce::File& src, int seed);
    std::shared_ptr<StretchedClip> loadResampled (const juce::File& f, double maxSeconds, int& outFileCh);
    static void pitchShiftBuffer (juce::AudioBuffer<float>& buf, double semitones); // granulaire

    // export helpers.
    juce::int64 commonExportLen() const;            // longueur commune (bar-aligned)
    bool writeClipAligned (const StretchedClip& clip, juce::int64 totalLen, const juce::File& outFile) const;
    juce::String stemFileName (int slotIndex) const;

    // index incrémental (sidecar .memento_index.json dans la racine).
    juce::File indexFile() const;
    void loadIndex (std::map<juce::String, Record>& out) const;
    void saveIndex (const std::vector<Record>& recs) const;

    // --- données ---
    juce::File              libraryFolder;
    std::vector<Record>     records;
    std::vector<std::unique_ptr<Slot>> slots;
    std::atomic<double>     hostBpm { 124.0 };
    double                  sr = 44100.0;
    std::atomic<bool>       scanning { false };
    std::atomic<int>        lastNew { 0 };

    Key                     projectKey;             // cible d'accordage
    std::atomic<bool>       keySync { false };
    mutable juce::CriticalSection keyLock;

    juce::File              stylesFolder;
    std::vector<StyleInfo>  styleList;

    juce::AudioFormatManager formatManager;
    mutable juce::Random    rng;

    mutable juce::CriticalSection dataLock;
    mutable juce::CriticalSection slotsLock;
    mutable juce::CriticalSection stylesLock;

    juce::CriticalSection   jobLock;
    std::vector<int>        jobQueue;
    juce::File              pendingScan;
    std::atomic<bool>       pendingStyleRescan { false };
    juce::WaitableEvent     jobEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MementoEngine)
};

} // namespace mem

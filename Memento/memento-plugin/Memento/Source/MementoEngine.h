#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

// ===========================================================================
// Memento — moteur d'assemblage (v0.1).
//   • SampleLibrary : scan récursif d'un dossier, parsing BPM + rôle depuis le
//     nom de fichier (repris de l'app Electron Memento).
//   • Assembler     : slots par rôle, reroll/lock, rendu offline calé au tempo
//     de l'hôte (SoundTouch), et mixage multi-voix synchronisé dans process().
// Le rendu (chargement + time-stretch) tourne sur un thread de fond ; le thread
// audio ne fait que lire des buffers déjà prêts (échange atomique de pointeur).
// ===========================================================================

namespace mem
{

enum class Role { Drums, Perc, Bass, Tonal, Texture, Vox, Fx, NumRoles };

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
        default:            return "?";
    }
}

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

// Un slot d'assemblage.
struct Slot
{
    Role role = Role::Drums;
    int  recordIndex = -1;

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

    // --- tempo hôte ---
    void setHostBpm (double bpm);
    double getHostBpm() const { return hostBpm.load(); }

    // --- slots ---
    int  getNumSlots() const { return (int) slots.size(); }
    Slot* getSlot (int i) { return (i >= 0 && i < (int) slots.size()) ? slots[(size_t) i].get() : nullptr; }
    void  setDefaultSlots();                 // Drums/Bass/Tonal/Texture/Vox
    void  addSlot (Role role);
    void  removeSlot (int index);

    void  rerollSlot (int index);
    void  rerollAll();                       // slots non verrouillés

    // --- lecture (thread audio) ---
    // hostPosSamples = position de lecture de l'hôte convertie en échantillons
    // (ppq * samplesParBeat) ; garantit un calage sur la grille de mesures.
    void  process (juce::AudioBuffer<float>& out, juce::int64 hostPosSamples, bool playing);

    // --- persistance (chemin dossier + rôles des slots) ---
    juce::String saveStateString() const;
    void         loadStateString (const juce::String& s);

    std::function<void()> onLibraryChanged;   // notif UI (thread message)
    std::function<void()> onSlotsChanged;     // notif UI

private:
    // Thread de rendu.
    void run() override;
    void doScan (const juce::File& folder);
    void requestRender (int slotIndex);
    void requestRenderAll();
    void renderSlot (int slotIndex);

    // Sélection.
    std::vector<int> candidatesForRole (Role role) const;
    int  pickForRole (Role role, int avoidIndex) const;

    // Rendu offline (chargement + stretch + resample).
    std::shared_ptr<StretchedClip> makeClip (const Record& rec);

    // --- données ---
    juce::File              libraryFolder;
    std::vector<Record>     records;                 // stable après scan
    std::vector<std::unique_ptr<Slot>> slots;
    std::atomic<double>     hostBpm { 124.0 };
    double                  sr = 44100.0;
    std::atomic<bool>       scanning { false };

    juce::AudioFormatManager formatManager;
    mutable juce::Random    rng;

    // Verrous : données (records) et structure des slots.
    mutable juce::CriticalSection dataLock;
    mutable juce::CriticalSection slotsLock;

    // File de rendu (indices de slots à (re)rendre) + scan en attente.
    juce::CriticalSection   jobLock;
    std::vector<int>        jobQueue;
    juce::File              pendingScan;
    juce::WaitableEvent     jobEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MementoEngine)
};

} // namespace mem

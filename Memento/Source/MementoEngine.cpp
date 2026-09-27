#include "MementoEngine.h"
#include <regex>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <numeric>

using namespace juce;

namespace mem
{

// --- extensions audio reconnues ------------------------------------------------
static bool isAudioFile (const File& f)
{
    static const StringArray exts { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a" };
    return exts.contains (f.getFileExtension().toLowerCase());
}

// --- table mots-clés → rôle (repris de l'app Memento) --------------------------
static Role roleFromName (const String& fileName, const String& parentName)
{
    auto hay = (fileName + " " + parentName).toLowerCase();
    auto has = [&] (std::initializer_list<const char*> ks) {
        for (auto* k : ks) if (hay.contains (k)) return true;
        return false;
    };
    if (has({ "kick", "snare", "clap", "hihat", "hi-hat", " hat", "cymbal", "crash", "ride", " tom", "drum" })) return Role::Drums;
    if (has({ "perc", "conga", "bongo", "shaker", "tamb", "cowbell", "djembe", "timbale" }))                    return Role::Perc;
    if (has({ "808", "sub bass", "subbass", "bass" }))                                                          return Role::Bass;
    if (has({ "vocal", "vox", "acap", "adlib", "ad-lib", "choir", "chant" }))                                   return Role::Vox;
    if (has({ "fx", "riser", "uplifter", "downlifter", "impact", "sweep", "whoosh", "noise", "transition" }))   return Role::Fx;
    if (has({ "texture", "ambience", "ambient", "atmos", "drone", "pad" }))                                     return Role::Texture;
    if (has({ "synth", "lead", "pluck", "arp", "chord", "piano", "keys", "rhodes", "guitar", "brass", "horn",
              "sax", "string", "violin", "cello", "melod" }))                                                   return Role::Tonal;
    return Role::Tonal; // défaut
}

static int parseBpm (const String& name)
{
    static const std::regex re (R"((\d{2,3})\s?[-_ ]?\s?bpm)", std::regex::icase);
    std::smatch m;
    auto s = name.toStdString();
    if (std::regex_search (s, m, re)) {
        int v = std::atoi (m[1].str().c_str());
        if (v >= 40 && v <= 220) return v;
    }
    return 0;
}

static int snapBeats (double beats)
{
    static const int table[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
    if (! (beats > 0.0)) return 4;
    int best = table[0]; double bestErr = 1e9;
    for (int b : table) { double e = std::abs (std::log2 ((double) b / beats)); if (e < bestErr) { bestErr = e; best = b; } }
    return best;
}

// ===========================================================================
// KeyParser — détection de tonalité depuis un nom de fichier
//   • formats acceptés : Am, Amin, A min, A Minor, A-minor, F#m, Dbm, Gbmaj…
//   • note seule : F#  → Root = F#, Mode = Unknown (on n'invente pas le mode)
//   • Camelot    : 8A → A minor, 8B → C major (I/O seulement)
//   • enharmonie : C# == Db (tout ramené en classes de hauteur 0..11)
//   Aucune tonalité codée en dur : tout est calculé.
// ===========================================================================

// C=0, C#=1, D=2 … B=11
int KeyParser::rootFromToken (const String& tokIn)
{
    auto tok = tokIn.trim();
    if (tok.isEmpty()) return -1;

    const juce::juce_wchar c = CharacterFunctions::toUpperCase (tok[0]);
    int base;
    switch (c) {
        case 'C': base = 0;  break;
        case 'D': base = 2;  break;
        case 'E': base = 4;  break;
        case 'F': base = 5;  break;
        case 'G': base = 7;  break;
        case 'A': base = 9;  break;
        case 'B': base = 11; break;
        default: return -1;
    }
    // accidents éventuels juste après la lettre
    for (int i = 1; i < tok.length(); ++i)
    {
        const juce::juce_wchar a = tok[i];
        if (a == '#' || a == 's' || a == 'S')      base += 1;
        else if (a == 'b' || a == 'B' || a == 'f') base -= 1;   // 'b'/'f' = bémol
        else break;
    }
    return ((base % 12) + 12) % 12;
}

// noms de note (dièses par défaut)
String KeyParser::rootName (int root, bool preferFlat)
{
    if (root < 0) return "?";
    static const char* sharp[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    static const char* flat[]  = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
    root = ((root % 12) + 12) % 12;
    return preferFlat ? flat[root] : sharp[root];
}

String KeyParser::toString (const Key& k)
{
    if (! k.hasRoot()) return String::fromUTF8 ("\xE2\x80\x94"); // —
    String r = rootName (k.root);
    if (k.mode == Mode::Minor) return r + " Minor";
    if (k.mode == Mode::Major) return r + " Major";
    return r + " (root)";
}

// Camelot → (root, mode). Index 0 inutilisé ; 1..12.
static Key camelotToKey (int number, bool sideB)
{
    // roues standard : A = mineur, B = majeur
    static const int minorRoot[13] = { -1, 8,3,10,5,0,7,2,9,4,11,6,1 }; // 1A..12A
    static const int majorRoot[13] = { -1, 11,6,1,8,3,10,5,0,7,2,9,4 }; // 1B..12B
    Key k;
    if (number < 1 || number > 12) return k;
    if (sideB) { k.root = majorRoot[number]; k.mode = Mode::Major; }
    else       { k.root = minorRoot[number]; k.mode = Mode::Minor; }
    return k;
}

// un token est-il un mot de mode ? -1 = non, 0 = major, 1 = minor
static int modeWord (const String& tokIn)
{
    auto t = tokIn.toLowerCase();
    if (t == "min" || t == "minor" || t == "m" || t == "moll") return 1;
    if (t == "maj" || t == "major" || t == "dur")              return 0;
    return -1;
}

// essaie de parser un token "clé collée" : Am, Amin, AMinor, F#m, Dbmaj, Gbmin
static Key keyFromGluedToken (const String& tok)
{
    Key k;
    if (tok.isEmpty()) return k;
    const juce::juce_wchar c = CharacterFunctions::toUpperCase (tok[0]);
    if (! (c >= 'A' && c <= 'G')) return k;

    // longueur de la racine (lettre + accidents)
    int p = 1;
    while (p < tok.length())
    {
        const juce::juce_wchar a = tok[p];
        if (a == '#' || a == 's' || a == 'S' || a == 'b' || a == 'f') ++p;
        else break;
    }
    // Attention : "b" peut être un bémol OU le début de "b..." — on borne aux accidents.
    String rootTok = tok.substring (0, p);
    String rest    = tok.substring (p);

    int root = KeyParser::rootFromToken (rootTok);
    if (root < 0) return k;

    if (rest.isEmpty())
    {
        // note seule → Root connu, Mode inconnu (on n'invente pas)
        k.root = root; k.mode = Mode::Unknown;
        return k;
    }
    int mw = modeWord (rest);
    if (mw == 1) { k.root = root; k.mode = Mode::Minor; return k; }
    if (mw == 0) { k.root = root; k.mode = Mode::Major; return k; }

    // reste non reconnu → pas une clé fiable (ex. "Bass")
    return Key();
}

Key KeyParser::fromName (const String& name)
{
    // découpe en tokens sur les séparateurs usuels
    StringArray toks;
    toks.addTokens (name, " _-.()[]", "");
    toks.removeEmptyStrings();

    Key best;          // meilleure trouvaille
    bool bestFull = false;

    for (int i = 0; i < toks.size(); ++i)
    {
        const auto& t = toks[i];

        // 1) Camelot : ^(1..12)(A|B)$
        {
            static const std::regex cam (R"(^([0-9]{1,2})([ABab])$)");
            std::smatch mm; auto s = t.toStdString();
            if (std::regex_match (s, mm, cam))
            {
                int num = std::atoi (mm[1].str().c_str());
                bool sideB = (mm[2].str()[0] == 'B' || mm[2].str()[0] == 'b');
                Key ck = camelotToKey (num, sideB);
                if (ck.isFull()) return ck; // très haute confiance
            }
        }

        // 2) clé collée : Am / A#min / Dbmaj / F#
        Key g = keyFromGluedToken (t);
        if (g.isFull()) return g;               // clé complète → on prend
        if (g.hasRoot())
        {
            // note seule : peut être complétée par un mot de mode suivant
            if (i + 1 < toks.size())
            {
                int mw = modeWord (toks[i + 1]);
                if (mw == 1) { g.mode = Mode::Minor; return g; }
                if (mw == 0) { g.mode = Mode::Major; return g; }
            }
            if (! bestFull && ! best.hasRoot()) best = g; // garde en repli (Root seul)
        }
    }
    return best; // éventuellement Root seul, sinon vide
}

// ---------------------------------------------------------------------------

MementoEngine::MementoEngine() : juce::Thread ("Memento Render")
{
    formatManager.registerBasicFormats();
    startThread(); // thread de rendu de fond
}

MementoEngine::~MementoEngine()
{
    signalThreadShouldExit();
    jobEvent.signal();
    stopThread (2000);
}

void MementoEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    requestRenderAll();
}

void MementoEngine::setHostBpm (double bpm)
{
    if (bpm >= 40.0 && bpm <= 300.0)
        hostBpm.store (bpm);
}

int MementoEngine::getRecordCount() const
{
    const ScopedLock sl (dataLock);
    return (int) records.size();
}

int MementoEngine::getUsableCount() const
{
    const ScopedLock sl (dataLock);
    int n = 0;
    for (auto& r : records) if (r.isLoop && r.bpm > 0) ++n;
    return n;
}

String MementoEngine::getStatusText() const
{
    if (scanning.load()) return String::fromUTF8 ("Scanning library\xE2\x80\xA6");
    const int total = getRecordCount();
    if (total == 0) return String::fromUTF8 ("No library");
    const int nw = lastNew.load();
    String s = String::fromUTF8 ("Library ready \xE2\x80\x94 ") + String (total) + " samples";
    if (nw > 0) s += " (" + String (nw) + " new)";
    return s;
}

// --- tonalité globale ------------------------------------------------------
void MementoEngine::setProjectKey (const Key& k)
{
    { const ScopedLock sl (keyLock); projectKey = k; }
    recomputeAllTuning();
}

Key MementoEngine::getProjectKey() const
{
    const ScopedLock sl (keyLock);
    return projectKey;
}

void MementoEngine::setKeySync (bool on)
{
    keySync.store (on);
    recomputeAllTuning();
}

// --- accordage par slot ----------------------------------------------------
void MementoEngine::setSlotTuneMode (int slotIndex, TuneMode m)
{
    if (auto* s = getSlot (slotIndex))
    {
        s->tuneMode.store ((int) m);
        recomputeSlotTuning (slotIndex);
        requestRender (slotIndex);
    }
}

void MementoEngine::setSlotManualSemis (int slotIndex, int semitones)
{
    if (auto* s = getSlot (slotIndex))
    {
        s->tuneMode.store ((int) TuneMode::Manual);
        s->semis.store (jlimit (-24, 24, semitones));
        s->cents.store (0);
        requestRender (slotIndex);
    }
}

TuneMode MementoEngine::getSlotTuneMode (int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < (int) slots.size())
        return (TuneMode) slots[(size_t) slotIndex]->tuneMode.load();
    return TuneMode::Auto;
}

// transposition minimisée dans [-6..+6] demi-tons (mode ignoré : distance de racine)
int MementoEngine::transposeSemitones (const Key& from, const Key& to)
{
    if (! from.hasRoot() || ! to.hasRoot()) return 0;
    int d = ((to.root - from.root) % 12 + 12) % 12; // 0..11
    if (d > 6) d -= 12;                              // -6..+6
    return d;
}

void MementoEngine::recomputeSlotTuning (int slotIndex)
{
    Slot* s = getSlot (slotIndex);
    if (s == nullptr) return;

    const TuneMode m = (TuneMode) s->tuneMode.load();

    if (m == TuneMode::Original || ! s->tonal.load())
    {
        s->semis.store (0);
        s->cents.store (0);
        return;
    }
    if (m == TuneMode::Manual)
        return; // semis fixés par l'utilisateur

    // Auto : suit la PROJECT KEY si KEY SYNC actif
    int semi = 0;
    if (keySync.load())
    {
        Key pk; { const ScopedLock sl (keyLock); pk = projectKey; }
        if (pk.hasRoot() && s->sampleKey.hasRoot())
            semi = transposeSemitones (s->sampleKey, pk);
    }
    s->semis.store (semi);
    s->cents.store (0);
}

void MementoEngine::recomputeAllTuning()
{
    const int n = getNumSlots();
    for (int i = 0; i < n; ++i) recomputeSlotTuning (i);
    requestRenderAll();
    MessageManager::callAsync ([this] { if (onSlotsChanged) onSlotsChanged(); });
}

// --- scan bibliothèque -----------------------------------------------------
void MementoEngine::scanFolder (const File& folder)
{
    if (! folder.isDirectory()) return;
    libraryFolder = folder;
    scanning.store (true);
    { const ScopedLock sl (jobLock); pendingScan = folder; }
    jobEvent.signal();
}

// index incrémental --------------------------------------------------------
File MementoEngine::indexFile() const
{
    return libraryFolder.getChildFile (".memento_index.json");
}

void MementoEngine::loadIndex (std::map<String, Record>& out) const
{
    auto f = indexFile();
    if (! f.existsAsFile()) return;
    var root = JSON::parse (f);
    if (auto* arr = root.getProperty ("items", var()).getArray())
    {
        for (auto& v : *arr)
        {
            Record r;
            String path = v.getProperty ("path", "").toString();
            if (path.isEmpty()) continue;
            r.mtime  = (int64) (double) v.getProperty ("mtime", 0.0);
            r.size   = (int64) (double) v.getProperty ("size", 0.0);
            r.role   = (Role) (int) v.getProperty ("role", (int) Role::Tonal);
            r.bpm    = (int) v.getProperty ("bpm", 0);
            r.isLoop = (bool) v.getProperty ("loop", false);
            r.key.root = (int) v.getProperty ("kroot", -1);
            r.key.mode = (Mode) (int) v.getProperty ("kmode", (int) Mode::Unknown);
            out[path] = r;
        }
    }
}

void MementoEngine::saveIndex (const std::vector<Record>& recs) const
{
    var root (new DynamicObject());
    Array<var> arr;
    for (auto& r : recs)
    {
        var o (new DynamicObject());
        o.getDynamicObject()->setProperty ("path",  r.file.getFullPathName());
        o.getDynamicObject()->setProperty ("mtime", (double) r.mtime);
        o.getDynamicObject()->setProperty ("size",  (double) r.size);
        o.getDynamicObject()->setProperty ("role",  (int) r.role);
        o.getDynamicObject()->setProperty ("bpm",   r.bpm);
        o.getDynamicObject()->setProperty ("loop",  r.isLoop);
        o.getDynamicObject()->setProperty ("kroot", r.key.root);
        o.getDynamicObject()->setProperty ("kmode", (int) r.key.mode);
        arr.add (o);
    }
    root.getDynamicObject()->setProperty ("items", arr);
    indexFile().replaceWithText (JSON::toString (root, false));
}

void MementoEngine::doScan (const File& folder)
{
    std::map<String, Record> index;
    loadIndex (index);

    std::vector<Record> tmp;
    int newCount = 0;

    for (const auto& f : RangedDirectoryIterator (folder, true, "*", File::findFiles))
    {
        if (threadShouldExit()) return;
        auto file = f.getFile();
        if (! isAudioFile (file)) continue;

        const String path  = file.getFullPathName();
        const int64  mtime = file.getLastModificationTime().toMilliseconds();
        const int64  size  = file.getSize();

        auto it = index.find (path);
        if (it != index.end() && it->second.mtime == mtime && it->second.size == size)
        {
            // inchangé → on réutilise l'analyse mise en cache
            Record r = it->second;
            r.file = file;
            r.name = file.getFileNameWithoutExtension();
            r.pack = file.getParentDirectory().getFileName();
            tmp.push_back (std::move (r));
        }
        else
        {
            // nouveau ou modifié → (ré)analyse
            Record r = recordFromFile (file);
            r.mtime = mtime;
            r.size  = size;
            tmp.push_back (std::move (r));
            ++newCount;
        }
    }

    lastNew.store (newCount);
    {
        const ScopedLock sl (dataLock);
        records = std::move (tmp);
    }
    saveIndex (records);   // supprime implicitement les fichiers disparus
    scanning.store (false);

    // Les styles peuvent vivre dans un sous-dossier de la bibliothèque.
    pendingStyleRescan.store (true);
    jobEvent.signal();

    MessageManager::callAsync ([this]
    {
        if (slots.empty()) setDefaultSlots();
        rerollAll();
        if (onLibraryChanged) onLibraryChanged();
        if (onSlotsChanged)   onSlotsChanged();
    });
}

// --- styles ----------------------------------------------------------------
void MementoEngine::setStylesFolder (const File& folder)
{
    stylesFolder = folder;
    pendingStyleRescan.store (true);
    jobEvent.signal();
}

void MementoEngine::doStyleScan()
{
    auto hasSubdirs = [] (const File& d) {
        return d.isDirectory() && d.getNumberOfChildFiles (File::findDirectories) > 0;
    };

    File root = stylesFolder;
    if (! hasSubdirs (root))
    {
        File alt = libraryFolder.getChildFile ("styles");
        if (hasSubdirs (alt)) root = alt;
    }

    std::vector<StyleInfo> tmp;
    if (root.isDirectory())
    {
        for (const auto& sub : RangedDirectoryIterator (root, false, "*", File::findDirectories))
        {
            if (threadShouldExit()) return;
            StyleInfo si;
            si.dir  = sub.getFile();
            si.name = si.dir.getFileName();
            for (const auto& f : RangedDirectoryIterator (si.dir, true, "*", File::findFiles))
            {
                auto file = f.getFile();
                if (isAudioFile (file)) si.files.add (file);
            }
            if (! si.files.isEmpty()) tmp.push_back (std::move (si));
        }
    }
    {
        const ScopedLock sl (stylesLock);
        styleList = std::move (tmp);
    }
    MessageManager::callAsync ([this] { if (onStylesChanged) onStylesChanged(); });
}

StringArray MementoEngine::getStyleNames() const
{
    StringArray names;
    const ScopedLock sl (stylesLock);
    for (auto& s : styleList) names.add (s.name);
    return names;
}

Array<File> MementoEngine::filesForStyle (const String& style) const
{
    const ScopedLock sl (stylesLock);
    for (auto& s : styleList) if (s.name == style) return s.files;
    return {};
}

// --- slots -----------------------------------------------------------------
void MementoEngine::setDefaultSlots()
{
    const ScopedLock sl (slotsLock);
    slots.clear();
    for (Role r : { Role::Drums, Role::Bass, Role::Tonal, Role::Texture, Role::Vox })
    {
        auto s = std::make_unique<Slot>();
        s->role = r;
        s->source = SlotSource::Library;
        s->tonal.store (roleIsTonal (r));
        slots.push_back (std::move (s));
    }
}

void MementoEngine::addSlot (Role role)
{
    int idx;
    {
        const ScopedLock sl (slotsLock);
        auto s = std::make_unique<Slot>();
        s->role = role;
        s->source = SlotSource::Library;
        s->tonal.store (roleIsTonal (role));
        slots.push_back (std::move (s));
        idx = (int) slots.size() - 1;
    }
    rerollSlot (idx);
    if (onSlotsChanged) onSlotsChanged();
}

void MementoEngine::addStyleLoopSlot (const String& style)
{
    int idx;
    {
        const ScopedLock sl (slotsLock);
        auto s = std::make_unique<Slot>();
        s->role = Role::Tonal;
        s->source = SlotSource::StyleLoop;
        s->styleName = style;
        s->displayName = "Boucle \xC2\xB7 " + style;
        slots.push_back (std::move (s));
        idx = (int) slots.size() - 1;
    }
    rerollSlot (idx);
    if (onSlotsChanged) onSlotsChanged();
}

void MementoEngine::addRollSlot (const String& style)
{
    int idx;
    {
        const ScopedLock sl (slotsLock);
        auto s = std::make_unique<Slot>();
        s->role = Role::Roll;
        s->source = SlotSource::Roll;
        s->styleName = style;
        s->tonal.store (false); // un roll rythmique n'est jamais pitché
        s->displayName = "Roll \xC2\xB7 " + style;
        slots.push_back (std::move (s));
        idx = (int) slots.size() - 1;
    }
    rerollSlot (idx);
    if (onSlotsChanged) onSlotsChanged();
}

void MementoEngine::removeSlot (int index)
{
    {
        const ScopedLock sl (slotsLock);
        if (index >= 0 && index < (int) slots.size())
            slots.erase (slots.begin() + index);
    }
    if (onSlotsChanged) onSlotsChanged();
}

// --- sélection (Library) ---------------------------------------------------
std::vector<int> MementoEngine::candidatesForRole (Role role) const
{
    std::vector<int> loops, all;
    const ScopedLock sl (dataLock);
    for (int i = 0; i < (int) records.size(); ++i)
    {
        if (records[(size_t) i].role != role) continue;
        all.push_back (i);
        if (records[(size_t) i].isLoop && records[(size_t) i].bpm > 0) loops.push_back (i);
    }
    const bool layered = (role != Role::Fx && role != Role::Vox);
    if (layered && loops.size() >= 4) return loops;
    return all.empty() ? loops : all;
}

int MementoEngine::pickForRole (Role role, int avoidIndex) const
{
    auto pool = candidatesForRole (role);
    if (pool.empty()) return -1;
    for (int tries = 0; tries < 8; ++tries)
    {
        int idx = pool[(size_t) rng.nextInt ((int) pool.size())];
        if (idx != avoidIndex || pool.size() == 1) return idx;
    }
    return pool[0];
}

// --- reroll : (re)génère selon la source -----------------------------------
void MementoEngine::rerollSlot (int index)
{
    Slot* s = getSlot (index);
    if (s == nullptr) return;

    if (s->source == SlotSource::Library)
    {
        int pick = pickForRole (s->role, s->recordIndex);
        if (pick < 0) { s->displayName = "Aucun son"; if (onSlotsChanged) onSlotsChanged(); return; }
        s->recordIndex = pick;
        {
            const ScopedLock sl (dataLock);
            const auto& rec = records[(size_t) pick];
            s->displayName = rec.name;
            s->sampleKey   = rec.key;
            s->tonal.store (roleIsTonal (rec.role));
            String sub = rec.pack + " \xC2\xB7 "
                       + (rec.bpm > 0 ? String (rec.bpm) + " BPM" : String ("libre"));
            if (rec.key.hasRoot()) sub += String::fromUTF8 (" \xC2\xB7 ") + KeyParser::toString (rec.key);
            s->displaySub = sub;
        }
    }
    else if (s->source == SlotSource::StyleLoop)
    {
        auto files = filesForStyle (s->styleName);
        if (files.isEmpty()) { s->displayName = "(style vide)"; if (onSlotsChanged) onSlotsChanged(); return; }
        s->fileSource  = files[rng.nextInt (files.size())];
        s->displayName = s->fileSource.getFileNameWithoutExtension();
        Record rec = recordFromFile (s->fileSource);
        s->sampleKey = rec.key;
        s->tonal.store (roleIsTonal (rec.role));
        String sub = s->styleName + " \xC2\xB7 boucle";
        if (rec.key.hasRoot()) sub += String::fromUTF8 (" \xC2\xB7 ") + KeyParser::toString (rec.key);
        s->displaySub = sub;
    }
    else // Roll
    {
        auto files = filesForStyle (s->styleName);
        if (files.isEmpty()) { s->displayName = "(style vide)"; if (onSlotsChanged) onSlotsChanged(); return; }
        s->fileSource  = files[rng.nextInt (files.size())];
        s->rollSeed    = rng.nextInt();
        s->tonal.store (false);
        s->sampleKey   = Key();
        s->displayName = "Roll \xC2\xB7 " + s->styleName;
        s->displaySub  = s->fileSource.getFileNameWithoutExtension() + " \xC2\xB7 variation";
    }

    recomputeSlotTuning (index);
    s->rendering.store (true);
    requestRender (index);
    if (onSlotsChanged) onSlotsChanged();
}

void MementoEngine::rerollAll()
{
    for (int i = 0; i < getNumSlots(); ++i)
    {
        Slot* s = getSlot (i);
        if (s != nullptr && ! s->locked.load()) rerollSlot (i);
    }
}

// --- file de rendu ---------------------------------------------------------
void MementoEngine::requestRender (int slotIndex)
{
    { const ScopedLock sl (jobLock); jobQueue.push_back (slotIndex); }
    jobEvent.signal();
}
void MementoEngine::requestRenderAll()
{
    { const ScopedLock sl (jobLock); for (int i = 0; i < getNumSlots(); ++i) jobQueue.push_back (i); }
    jobEvent.signal();
}

void MementoEngine::run()
{
    double lastRenderedBpm = hostBpm.load();
    while (! threadShouldExit())
    {
        jobEvent.wait (400);
        if (threadShouldExit()) return;

        // scan bibliothèque en attente ?
        File toScan;
        { const ScopedLock sl (jobLock); toScan = pendingScan; pendingScan = File(); }
        if (toScan.isDirectory()) { doScan (toScan); lastRenderedBpm = hostBpm.load(); }

        // scan des styles en attente ?
        if (pendingStyleRescan.exchange (false)) doStyleScan();

        // changement de tempo → re-rendu global
        double bpm = hostBpm.load();
        if (std::abs (bpm - lastRenderedBpm) > 0.4)
        {
            lastRenderedBpm = bpm;
            requestRenderAll();
        }

        // drainer la file
        for (;;)
        {
            int idx = -1;
            { const ScopedLock sl (jobLock); if (! jobQueue.empty()) { idx = jobQueue.front(); jobQueue.erase (jobQueue.begin()); } }
            if (idx < 0) break;
            if (threadShouldExit()) return;
            renderSlot (idx);
        }
    }
}

void MementoEngine::renderSlot (int slotIndex)
{
    // Copie des infos nécessaires sous verrou (le slot peut disparaître).
    SlotSource src = SlotSource::Library;
    int   recIndex = -1;
    File  fsrc;
    int   seed = 0;
    bool  tonal = false;
    int   semis = 0, cents = 0;
    {
        const ScopedLock sl (slotsLock);
        if (slotIndex < 0 || slotIndex >= (int) slots.size()) return;
        auto* s = slots[(size_t) slotIndex].get();
        src = s->source; recIndex = s->recordIndex; fsrc = s->fileSource; seed = s->rollSeed;
        tonal = s->tonal.load(); semis = s->semis.load(); cents = s->cents.load();
    }

    std::shared_ptr<StretchedClip> clip;
    if (src == SlotSource::Library)
    {
        if (recIndex < 0) return;
        Record rec;
        { const ScopedLock sl (dataLock); if (recIndex >= (int) records.size()) return; rec = records[(size_t) recIndex]; }
        clip = makeClip (rec);
    }
    else if (src == SlotSource::StyleLoop)
    {
        if (! fsrc.existsAsFile()) return;
        clip = makeClip (recordFromFile (fsrc));
    }
    else // Roll
    {
        if (! fsrc.existsAsFile()) return;
        clip = makeRollClip (fsrc, seed);
    }

    // Accordage : pitch-shift granulaire (préserve la durée → BPM inchangé),
    // uniquement pour les slots tonals qui demandent un décalage.
    if (clip != nullptr && tonal && (semis != 0 || cents != 0))
        pitchShiftBuffer (clip->buffer, (double) semis + (double) cents / 100.0);

    // Stocke le clip si le slot pointe toujours la même source.
    {
        const ScopedLock sl (slotsLock);
        if (slotIndex >= 0 && slotIndex < (int) slots.size())
        {
            auto* s = slots[(size_t) slotIndex].get();
            const bool same = (s->source == src)
                            && (src == SlotSource::Library ? (s->recordIndex == recIndex)
                                                           : (s->fileSource == fsrc && s->rollSeed == seed));
            if (same)
            {
                std::atomic_store (&s->clip, clip);
                s->rendering.store (false);
            }
        }
    }
    MessageManager::callAsync ([this] { if (onSlotsChanged) onSlotsChanged(); });
}

// --- rendu offline : boucle calée au tempo (Library / StyleLoop) -----------
Record MementoEngine::recordFromFile (const File& f)
{
    Record r;
    r.file = f;
    r.name = f.getFileNameWithoutExtension();
    r.pack = f.getParentDirectory().getFileName();
    r.bpm  = parseBpm (r.name);
    r.role = roleFromName (r.name, r.pack);
    r.isLoop = (r.bpm > 0) || r.name.toLowerCase().contains ("loop");
    r.key  = KeyParser::fromName (r.name);
    r.mtime = f.getLastModificationTime().toMilliseconds();
    r.size  = f.getSize();
    return r;
}

std::shared_ptr<StretchedClip> MementoEngine::makeClip (const Record& rec)
{
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (rec.file));
    if (reader == nullptr) return nullptr;

    const int    fileCh   = (int) jlimit (1, 2, (int) reader->numChannels);
    const double fileSr   = reader->sampleRate > 0 ? reader->sampleRate : sr;
    const int64  maxFrames = (int64) (fileSr * 16.0);           // borne à 16 s
    const int64  frames   = jmin ((int64) reader->lengthInSamples, maxFrames);
    if (frames < 256) return nullptr;

    AudioBuffer<float> src ((int) reader->numChannels, (int) frames);
    reader->read (&src, 0, (int) frames, 0, true, true);

    const double srcBpm     = rec.bpm > 0 ? (double) rec.bpm : 0.0;
    const double bpm        = hostBpm.load();
    const double tempoRatio = (srcBpm > 0.0) ? jlimit (0.25, 4.0, bpm / srcBpm) : 1.0;

    // Calage au tempo par ré-échantillonnage : sortie i (SR hôte) ->
    // position source = i * tempoRatio * fileSr / hostSr.  (KEY et BPM restent
    // découplés : ici on ne touche qu'au TEMPS ; la hauteur est gérée à part
    // par pitchShiftBuffer après ce rendu.)
    const double step = tempoRatio * fileSr / sr;
    const int    srcN = src.getNumSamples();
    const int64  outFrames = (int64) std::floor ((double) (srcN - 1) / jmax (1.0e-6, step));
    if (outFrames < 64) return nullptr;

    auto clip = std::make_shared<StretchedClip>();
    clip->buffer.setSize (2, (int) outFrames, false, true, true);
    for (int64 i = 0; i < outFrames; ++i)
    {
        double sp = (double) i * step;
        int64 i0 = (int64) sp;
        int64 i1 = jmin (i0 + 1, (int64) srcN - 1);
        float frac = (float) (sp - (double) i0);
        for (int c = 0; c < 2; ++c)
        {
            int sc = jmin (c, fileCh - 1);
            float a = src.getSample (sc, (int) i0);
            float b = src.getSample (sc, (int) i1);
            clip->buffer.setSample (c, (int) i, a + (b - a) * frac);
        }
    }

    int64 loopLen = outFrames;
    if (srcBpm > 0.0)
    {
        double srcDurSec = (double) frames / fileSr;
        int beats = snapBeats (srcDurSec * srcBpm / 60.0);
        loopLen = (int64) std::llround ((double) beats * (60.0 / bpm) * sr);
    }
    clip->loopLen   = jlimit ((int64) 64, outFrames, loopLen);
    clip->name      = rec.name;
    clip->sourceBpm = rec.bpm;
    return clip;
}

// --- chargement d'un one-shot ré-échantillonné au SR hôte (sans stretch) ---
std::shared_ptr<StretchedClip> MementoEngine::loadResampled (const File& f, double maxSeconds, int& outFileCh)
{
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (f));
    if (reader == nullptr) return nullptr;

    const int    fileCh   = (int) jlimit (1, 2, (int) reader->numChannels);
    const double fileSr   = reader->sampleRate > 0 ? reader->sampleRate : sr;
    const int64  maxFrames = (int64) (fileSr * maxSeconds);
    const int64  frames   = jmin ((int64) reader->lengthInSamples, maxFrames);
    if (frames < 64) return nullptr;

    AudioBuffer<float> src ((int) reader->numChannels, (int) frames);
    reader->read (&src, 0, (int) frames, 0, true, true);

    const double step = fileSr / sr;    // SR uniquement (pitch préservé)
    const int    srcN = src.getNumSamples();
    const int64  outFrames = (int64) std::floor ((double) (srcN - 1) / jmax (1.0e-6, step));
    if (outFrames < 16) return nullptr;

    auto clip = std::make_shared<StretchedClip>();
    clip->buffer.setSize (2, (int) outFrames, false, true, true);
    for (int64 i = 0; i < outFrames; ++i)
    {
        double sp = (double) i * step;
        int64 i0 = (int64) sp;
        int64 i1 = jmin (i0 + 1, (int64) srcN - 1);
        float frac = (float) (sp - (double) i0);
        for (int c = 0; c < 2; ++c)
        {
            int sc = jmin (c, fileCh - 1);
            float a = src.getSample (sc, (int) i0);
            float b = src.getSample (sc, (int) i1);
            clip->buffer.setSample (c, (int) i, a + (b - a) * frac);
        }
    }
    clip->loopLen = outFrames;
    outFileCh = fileCh;
    return clip;
}

// --- rendu d'un roll rythmique (retrigger sur la grille + variation) -------
std::shared_ptr<StretchedClip> MementoEngine::makeRollClip (const File& src, int seed)
{
    int fileCh = 2;
    auto oneShot = loadResampled (src, 2.0, fileCh);   // one-shot au SR hôte
    if (oneShot == nullptr) return nullptr;
    const auto& s = oneShot->buffer;
    const int   srcN = s.getNumSamples();

    const double bpm = hostBpm.load();
    const int64  loopLen = (int64) std::llround (4.0 * (60.0 / bpm) * sr);   // 1 mesure
    if (loopLen < 64) return nullptr;

    Random r ((int64) seed);
    const bool accel = r.nextFloat() < 0.55f;   // build-up accéléré ou roll droit

    auto clip = std::make_shared<StretchedClip>();
    clip->buffer.setSize (2, (int) loopLen, false, true, true);
    clip->buffer.clear();

    // positions des coups sur la mesure
    std::vector<int64> hits;
    if (accel)
    {
        int n = 8 + r.nextInt (16);                 // 8..23 coups
        double p = 1.4 + (double) r.nextFloat() * 0.9;  // exposant d'accélération
        for (int k = 0; k < n; ++k)
        {
            double u = std::pow ((double) k / (double) n, p);
            hits.push_back ((int64) (u * (double) loopLen));
        }
    }
    else
    {
        int div = (r.nextBool() ? 8 : 16);          // 1/8 ou 1/16
        for (int k = 0; k < div; ++k)
            hits.push_back ((int64) ((double) k / (double) div * (double) loopLen));
    }

    const size_t nh = hits.size();
    for (size_t h = 0; h < nh; ++h)
    {
        int64 start = hits[h];
        int64 next  = (h + 1 < nh) ? hits[h + 1] : loopLen;
        int64 span  = jmax ((int64) 1, next - start);
        int64 len   = jmin ((int64) srcN, jmin (span, loopLen - start));
        if (len <= 0) continue;

        // crescendo pour le feel build-up (droit = gain constant) + variation de vélocité
        float velo = 0.85f + 0.15f * (r.nextFloat() - 0.5f) * 2.0f;   // ±15 % aléatoire
        float hitGain = (accel ? (0.45f + 0.55f * (float) h / (float) jmax ((size_t) 1, nh - 1)) : 0.9f) * velo;

        int64 tail = jmin ((int64) 256, len);       // court fade pour éviter les clics
        for (int64 i = 0; i < len; ++i)
        {
            float env = (i > len - tail) ? (float) (len - i) / (float) tail : 1.0f;
            for (int c = 0; c < 2; ++c)
                clip->buffer.addSample (c, (int) (start + i), s.getSample (c, (int) i) * hitGain * env);
        }
    }

    clip->loopLen   = loopLen;
    clip->name      = src.getFileNameWithoutExtension();
    clip->sourceBpm = 0;
    return clip;
}

// --- pitch-shift granulaire (overlap-add, préserve la durée) ---------------
// ratio de hauteur = 2^(semitones/12). On lit, dans chaque grain, l'entrée à
// une vitesse = ratio (change la hauteur), mais les grains sont placés à la
// même position temporelle en entrée et en sortie (la durée est préservée).
// Fenêtre de Hann + normalisation par la somme des fenêtres (bords propres).
void MementoEngine::pitchShiftBuffer (AudioBuffer<float>& buf, double semitones)
{
    if (std::abs (semitones) < 1.0e-4) return;
    const int N = buf.getNumSamples();
    const int C = buf.getNumChannels();
    if (N < 64 || C <= 0) return;

    const double ratio = std::pow (2.0, semitones / 12.0);
    const int grain = jmin (2048, jmax (256, N / 4));
    const int hop   = jmax (1, grain / 4);          // 75 % de recouvrement

    // fenêtre de Hann
    std::vector<float> win ((size_t) grain);
    for (int j = 0; j < grain; ++j)
        win[(size_t) j] = 0.5f - 0.5f * std::cos (2.0 * MathConstants<double>::pi * (double) j / (double) (grain - 1));

    AudioBuffer<float> out (C, N);
    out.clear();
    std::vector<float> norm ((size_t) N, 0.0f);

    for (int outStart = 0; outStart < N; outStart += hop)
    {
        const int inStart = outStart;               // base temporelle alignée
        for (int j = 0; j < grain; ++j)
        {
            const int oi = outStart + j;
            if (oi >= N) break;
            const double readPos = (double) inStart + (double) j * ratio;
            if (readPos < 0.0 || readPos >= (double) (N - 1)) continue;
            const int   i0  = (int) readPos;
            const int   i1  = i0 + 1;
            const float fr  = (float) (readPos - (double) i0);
            const float w   = win[(size_t) j];
            for (int c = 0; c < C; ++c)
            {
                const float a = buf.getSample (c, i0);
                const float b = buf.getSample (c, i1);
                out.addSample (c, oi, (a + (b - a) * fr) * w);
            }
            norm[(size_t) oi] += w;
        }
    }

    for (int i = 0; i < N; ++i)
    {
        const float g = norm[(size_t) i];
        if (g > 1.0e-4f)
            for (int c = 0; c < C; ++c)
                buf.setSample (c, i, out.getSample (c, i) / g);
        else
            for (int c = 0; c < C; ++c)
                buf.setSample (c, i, 0.0f);
    }
}

// --- export stems : longueur commune calée sur les mesures -----------------
static long long gcdLL (long long a, long long b) { while (b != 0) { long long t = a % b; a = b; b = t; } return a < 0 ? -a : a; }
static long long lcmLL (long long a, long long b) { if (a == 0 || b == 0) return 1; long long g = gcdLL (a, b); return (a / g) * b; }

int64 MementoEngine::commonExportLen() const
{
    const double bpm = hostBpm.load();
    const double barSamples = 4.0 * (60.0 / bpm) * sr;   // 1 mesure 4/4
    if (barSamples < 1.0) return 0;

    long long lcmBars = 1;
    {
        const ScopedLock sl (slotsLock);
        for (auto& sp : slots)
        {
            auto clip = std::atomic_load (&sp->clip);
            if (clip == nullptr || clip->loopLen <= 0) continue;
            long long bars = (long long) jmax ((int64) 1,
                                (int64) std::llround ((double) clip->loopLen / barSamples));
            bars = jlimit ((long long) 1, (long long) 16, bars);
            lcmBars = lcmLL (lcmBars, bars);
            lcmBars = jmin (lcmBars, (long long) 16);     // borne de sécurité
        }
    }
    return (int64) std::llround ((double) lcmBars * barSamples);
}

// écrit un clip bouclé pour remplir exactement totalLen (même départ/durée
// pour tous les stems), en WAV 32-bit float au SR hôte.
bool MementoEngine::writeClipAligned (const StretchedClip& clip, int64 totalLen, const File& outFile) const
{
    if (totalLen <= 0) totalLen = clip.loopLen > 0 ? clip.loopLen : (int64) clip.buffer.getNumSamples();
    if (totalLen <= 0) return false;

    const int64 loopLen = clip.loopLen > 0 ? clip.loopLen : (int64) clip.buffer.getNumSamples();
    const int   clipN   = clip.buffer.getNumSamples();
    const int   clipCh  = clip.buffer.getNumChannels();
    if (loopLen <= 0 || clipN <= 0) return false;

    AudioBuffer<float> buf (2, (int) totalLen);
    buf.clear();
    for (int64 i = 0; i < totalLen; ++i)
    {
        const int64 src = i % loopLen;              // pavage (tiling) sur la boucle
        if (src >= clipN) continue;
        for (int c = 0; c < 2; ++c)
            buf.setSample (c, (int) i, clip.buffer.getSample (jmin (c, clipCh - 1), (int) src));
    }

    outFile.deleteFile();
    std::unique_ptr<FileOutputStream> fos (outFile.createOutputStream());
    if (fos == nullptr) return false;

    WavAudioFormat wav;   // 32 bits → WAVE_FORMAT_IEEE_FLOAT (aucune perte)
    std::unique_ptr<AudioFormatWriter> writer (wav.createWriterFor (fos.get(), sr, 2, 32, {}, 0));
    if (writer == nullptr) return false;
    fos.release(); // le writer prend possession du flux

    return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

String MementoEngine::stemFileName (int slotIndex) const
{
    Role role = Role::Drums;
    String nm;
    {
        const ScopedLock sl (slotsLock);
        if (slotIndex < 0 || slotIndex >= (int) slots.size()) return "Stem.wav";
        auto* s = slots[(size_t) slotIndex].get();
        role = s->role;
        nm   = s->displayName;
    }
    String base = String (slotIndex + 1).paddedLeft ('0', 2) + "_" + String (roleLabel (role));
    nm = nm.replace (String::fromUTF8 ("\xC2\xB7"), "-"); // retire le point médian
    nm = File::createLegalFileName (nm).trim();
    if (nm.isNotEmpty() && nm != "-" && nm != "?")
    {
        base += "_";
        base += nm.substring (0, 28);
    }
    return base + ".wav";
}

bool MementoEngine::exportStem (int slotIndex, const File& destDir)
{
    if (! destDir.isDirectory()) destDir.createDirectory();

    std::shared_ptr<StretchedClip> clip;
    {
        const ScopedLock sl (slotsLock);
        auto* s = getSlot (slotIndex);
        if (s == nullptr) return false;
        clip = std::atomic_load (&s->clip);
    }
    if (clip == nullptr || clip->buffer.getNumSamples() <= 0) return false;

    const int64 totalLen = commonExportLen();
    return writeClipAligned (*clip, totalLen, destDir.getChildFile (stemFileName (slotIndex)));
}

int MementoEngine::exportAllStems (const File& destDir)
{
    if (! destDir.isDirectory()) destDir.createDirectory();
    const int64 totalLen = commonExportLen();   // longueur commune calculée une fois
    int count = 0;
    const int n = getNumSlots();
    for (int i = 0; i < n; ++i)
    {
        std::shared_ptr<StretchedClip> clip;
        {
            const ScopedLock sl (slotsLock);
            auto* s = getSlot (i);
            if (s == nullptr) continue;
            clip = std::atomic_load (&s->clip);
        }
        if (clip == nullptr || clip->buffer.getNumSamples() <= 0) continue;
        if (writeClipAligned (*clip, totalLen, destDir.getChildFile (stemFileName (i)))) ++count;
    }
    return count;
}

// écrit un stem dans un fichier temporaire (pour le drag & drop vers le DAW)
File MementoEngine::writeStemToTemp (int slotIndex)
{
    std::shared_ptr<StretchedClip> clip;
    {
        const ScopedLock sl (slotsLock);
        auto* s = getSlot (slotIndex);
        if (s == nullptr) return File();
        clip = std::atomic_load (&s->clip);
    }
    if (clip == nullptr || clip->buffer.getNumSamples() <= 0) return File();

    File dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("MementoStems");
    dir.createDirectory();
    File out = dir.getChildFile (stemFileName (slotIndex));

    const int64 totalLen = commonExportLen();
    if (! writeClipAligned (*clip, totalLen, out)) return File();
    return out;
}

// --- lecture (thread audio) ------------------------------------------------
void MementoEngine::process (AudioBuffer<float>& out, int64 hostPosSamples, bool playing)
{
    out.clear();
    if (! playing) return;

    const ScopedTryLock stl (slotsLock);
    if (! stl.isLocked()) return; // structure en cours de modif → un bloc de silence

    bool anySolo = false;
    for (auto& sp : slots) if (sp->solo.load()) { anySolo = true; break; }

    const int nOut = out.getNumSamples();
    if (hostPosSamples < 0) hostPosSamples = 0;

    for (auto& sp : slots)
    {
        Slot* s = sp.get();
        if (s->mute.load()) continue;
        if (anySolo && ! s->solo.load()) continue;

        auto clip = std::atomic_load (&s->clip);
        if (clip == nullptr || clip->loopLen <= 0) continue;

        const float g   = s->gain.load();
        const float pan = s->pan.load();
        const float gl  = g * std::sqrt (0.5f * (1.0f - pan));
        const float gr  = g * std::sqrt (0.5f * (1.0f + pan));
        const int64 loopLen = clip->loopLen;
        const int   clipCh  = clip->buffer.getNumChannels();
        const int   clipN   = clip->buffer.getNumSamples();
        const float* cl = clip->buffer.getReadPointer (0);
        const float* cr = clip->buffer.getReadPointer (clipCh > 1 ? 1 : 0);
        float* ol  = out.getWritePointer (0);
        float* orr = out.getNumChannels() > 1 ? out.getWritePointer (1) : ol;

        for (int n = 0; n < nOut; ++n)
        {
            int64 pos = (hostPosSamples + n) % loopLen;
            if (pos >= clipN) continue;
            ol[n]  += cl[pos] * gl;
            orr[n] += cr[pos] * gr;
        }
    }
}

// --- persistance -----------------------------------------------------------
String MementoEngine::saveStateString() const
{
    var root (new DynamicObject());
    root.getDynamicObject()->setProperty ("folder", libraryFolder.getFullPathName());
    root.getDynamicObject()->setProperty ("styles", stylesFolder.getFullPathName());

    { // tonalité globale
        const ScopedLock sl (keyLock);
        root.getDynamicObject()->setProperty ("pkRoot", projectKey.root);
        root.getDynamicObject()->setProperty ("pkMode", (int) projectKey.mode);
    }
    root.getDynamicObject()->setProperty ("keySync", keySync.load());

    Array<var> arr;
    {
        const ScopedLock sl (slotsLock);
        for (auto& sp : slots)
        {
            var o (new DynamicObject());
            o.getDynamicObject()->setProperty ("role",   (int) sp->role);
            o.getDynamicObject()->setProperty ("source", (int) sp->source);
            o.getDynamicObject()->setProperty ("style",  sp->styleName);
            o.getDynamicObject()->setProperty ("file",   sp->fileSource.getFullPathName());
            o.getDynamicObject()->setProperty ("seed",   sp->rollSeed);
            o.getDynamicObject()->setProperty ("gain",  (double) sp->gain.load());
            o.getDynamicObject()->setProperty ("pan",   (double) sp->pan.load());
            o.getDynamicObject()->setProperty ("mute",  sp->mute.load());
            o.getDynamicObject()->setProperty ("solo",  sp->solo.load());
            o.getDynamicObject()->setProperty ("lock",  sp->locked.load());
            o.getDynamicObject()->setProperty ("tune",  sp->tuneMode.load());
            o.getDynamicObject()->setProperty ("semis", sp->semis.load());
            arr.add (o);
        }
    }
    root.getDynamicObject()->setProperty ("slots", arr);
    return JSON::toString (root, true);
}

void MementoEngine::loadStateString (const String& s)
{
    var root = JSON::parse (s);
    if (! root.isObject()) return;
    auto folder = File (root.getProperty ("folder", "").toString());
    auto styles = File (root.getProperty ("styles", "").toString());

    { // tonalité globale
        Key pk;
        pk.root = (int) root.getProperty ("pkRoot", -1);
        pk.mode = (Mode) (int) root.getProperty ("pkMode", (int) Mode::Unknown);
        const ScopedLock sl (keyLock);
        projectKey = pk;
    }
    keySync.store ((bool) root.getProperty ("keySync", false));

    if (auto* arr = root.getProperty ("slots", var()).getArray())
    {
        const ScopedLock sl (slotsLock);
        slots.clear();
        for (auto& v : *arr)
        {
            auto sp = std::make_unique<Slot>();
            sp->role   = (Role) (int) v.getProperty ("role", (int) Role::Tonal);
            sp->source = (SlotSource) (int) v.getProperty ("source", (int) SlotSource::Library);
            sp->styleName  = v.getProperty ("style", "").toString();
            sp->fileSource = File (v.getProperty ("file", "").toString());
            sp->rollSeed   = (int) v.getProperty ("seed", 0);
            sp->gain.store   ((float) (double) v.getProperty ("gain", 0.85));
            sp->pan.store    ((float) (double) v.getProperty ("pan", 0.0));
            sp->mute.store   ((bool) v.getProperty ("mute", false));
            sp->solo.store   ((bool) v.getProperty ("solo", false));
            sp->locked.store ((bool) v.getProperty ("lock", false));
            sp->tuneMode.store ((int) v.getProperty ("tune", (int) TuneMode::Auto));
            sp->semis.store  ((int) v.getProperty ("semis", 0));
            sp->tonal.store  (roleIsTonal (sp->role));
            slots.push_back (std::move (sp));
        }
    }
    if (styles.isDirectory()) setStylesFolder (styles);
    if (folder.isDirectory()) scanFolder (folder); // relance scan (rescan auto à l'ouverture) + première combinaison
}

} // namespace mem

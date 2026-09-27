#include "MementoEngine.h"
#include <regex>
#include <cmath>
#include <cstdlib>
#include <algorithm>

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

// --- scan bibliothèque -----------------------------------------------------
void MementoEngine::scanFolder (const File& folder)
{
    if (! folder.isDirectory()) return;
    libraryFolder = folder;
    scanning.store (true);
    { const ScopedLock sl (jobLock); pendingScan = folder; }
    jobEvent.signal();
}

void MementoEngine::doScan (const File& folder)
{
    std::vector<Record> tmp;
    for (const auto& f : RangedDirectoryIterator (folder, true, "*", File::findFiles))
    {
        if (threadShouldExit()) return;
        auto file = f.getFile();
        if (! isAudioFile (file)) continue;
        auto nm = file.getFileNameWithoutExtension();
        Record r;
        r.file = file;
        r.name = nm;
        r.pack = file.getParentDirectory().getFileName();
        r.bpm  = parseBpm (nm);
        r.role = roleFromName (nm, r.pack);
        r.isLoop = (r.bpm > 0) || nm.toLowerCase().contains ("loop");
        tmp.push_back (std::move (r));
    }
    {
        const ScopedLock sl (dataLock);
        records = std::move (tmp);
    }
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
        s->displayName = "Boucle · " + style;
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
        s->displayName = "Roll · " + style;
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
            s->displayName = records[(size_t) pick].name;
            s->displaySub  = records[(size_t) pick].pack + " · "
                           + (records[(size_t) pick].bpm > 0 ? String (records[(size_t) pick].bpm) + " BPM" : String ("libre"));
        }
        s->rendering.store (true);
        requestRender (index);
    }
    else if (s->source == SlotSource::StyleLoop)
    {
        auto files = filesForStyle (s->styleName);
        if (files.isEmpty()) { s->displayName = "(style vide)"; if (onSlotsChanged) onSlotsChanged(); return; }
        s->fileSource  = files[rng.nextInt (files.size())];
        s->displayName = s->fileSource.getFileNameWithoutExtension();
        s->displaySub  = s->styleName + " · boucle";
        s->rendering.store (true);
        requestRender (index);
    }
    else // Roll
    {
        auto files = filesForStyle (s->styleName);
        if (files.isEmpty()) { s->displayName = "(style vide)"; if (onSlotsChanged) onSlotsChanged(); return; }
        s->fileSource  = files[rng.nextInt (files.size())];
        s->rollSeed    = rng.nextInt();
        s->displayName = "Roll · " + s->styleName;
        s->displaySub  = s->fileSource.getFileNameWithoutExtension() + " · variation";
        s->rendering.store (true);
        requestRender (index);
    }

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
    {
        const ScopedLock sl (slotsLock);
        if (slotIndex < 0 || slotIndex >= (int) slots.size()) return;
        auto* s = slots[(size_t) slotIndex].get();
        src = s->source; recIndex = s->recordIndex; fsrc = s->fileSource; seed = s->rollSeed;
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

    // Calage au tempo par ré-échantillonnage (v0.1) : sortie i (SR hôte) ->
    // position source = i * tempoRatio * fileSr / hostSr.
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

        // crescendo pour le feel build-up (droit = gain constant)
        float hitGain = accel ? (0.45f + 0.55f * (float) h / (float) jmax ((size_t) 1, nh - 1)) : 0.9f;

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

// --- export stems ----------------------------------------------------------
bool MementoEngine::writeClipToWav (const StretchedClip& clip, const File& outFile) const
{
    const int64 n = clip.loopLen > 0 ? clip.loopLen : (int64) clip.buffer.getNumSamples();
    if (n <= 0) return false;

    AudioBuffer<float> buf (2, (int) n);
    buf.clear();
    const int avail  = clip.buffer.getNumSamples();
    const int copyN  = (int) jmin ((int64) avail, n);
    const int clipCh = clip.buffer.getNumChannels();
    for (int c = 0; c < 2; ++c)
        buf.copyFrom (c, 0, clip.buffer, jmin (c, clipCh - 1), 0, copyN);

    outFile.deleteFile();
    std::unique_ptr<FileOutputStream> fos (outFile.createOutputStream());
    if (fos == nullptr) return false;

    WavAudioFormat wav;
    // 32 bits → WAVE_FORMAT_IEEE_FLOAT (aucune perte de dynamique).
    std::unique_ptr<AudioFormatWriter> writer (wav.createWriterFor (fos.get(), sr, 2, 32, {}, 0));
    if (writer == nullptr) return false;
    fos.release(); // le writer prend possession du flux

    return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

bool MementoEngine::exportStem (int slotIndex, const File& destDir)
{
    if (! destDir.isDirectory()) destDir.createDirectory();

    std::shared_ptr<StretchedClip> clip;
    Role role = Role::Drums;
    String nm;
    {
        const ScopedLock sl (slotsLock);
        auto* s = getSlot (slotIndex);
        if (s == nullptr) return false;
        clip = std::atomic_load (&s->clip);
        role = s->role;
        nm   = s->displayName;
    }
    if (clip == nullptr || clip->buffer.getNumSamples() <= 0) return false;

    const int bpm = (int) std::llround (hostBpm.load());
    String base = "Memento_" + String (slotIndex + 1).paddedLeft ('0', 2)
                + "_" + String (roleLabel (role))
                + "_" + File::createLegalFileName (nm)
                + "_" + String (bpm) + "bpm";
    return writeClipToWav (*clip, destDir.getChildFile (base + ".wav"));
}

int MementoEngine::exportAllStems (const File& destDir)
{
    if (! destDir.isDirectory()) destDir.createDirectory();
    int count = 0;
    const int n = getNumSlots();
    for (int i = 0; i < n; ++i)
        if (exportStem (i, destDir)) ++count;
    return count;
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
            slots.push_back (std::move (sp));
        }
    }
    if (styles.isDirectory()) setStylesFolder (styles);
    if (folder.isDirectory()) scanFolder (folder); // relance scan + première combinaison
}

} // namespace mem

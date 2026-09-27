#include "MementoEngine.h"
#include <SoundTouch.h>
#include <regex>
#include <cmath>
#include <algorithm>

using namespace juce;

namespace mem
{

// --- table mots-clés → rôle (repris de l'app Memento) ----------------------
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
    if (has({ "vocal", "vox", "acap", "adlib", "ad-lib", "choir", "chant" }))                                    return Role::Vox;
    if (has({ "fx", "riser", "uplifter", "downlifter", "impact", "sweep", "whoosh", "noise", "transition" }))    return Role::Fx;
    if (has({ "texture", "ambience", "ambient", "atmos", "drone", "pad" }))                                       return Role::Texture;
    if (has({ "synth", "lead", "pluck", "arp", "chord", "piano", "keys", "rhodes", "guitar", "brass", "horn",
              "sax", "string", "violin", "cello", "melod" }))                                                     return Role::Tonal;
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

// --- scan ------------------------------------------------------------------
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
    const StringArray audioExts { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a" };
    for (const auto& f : RangedDirectoryIterator (folder, true, "*", File::findFiles))
    {
        if (threadShouldExit()) return;
        auto file = f.getFile();
        if (! audioExts.contains (file.getFileExtension().toLowerCase())) continue;
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

    // Sur le thread message : slots par défaut si besoin + première combinaison.
    MessageManager::callAsync ([this]
    {
        if (slots.empty()) setDefaultSlots();
        rerollAll();
        if (onLibraryChanged) onLibraryChanged();
        if (onSlotsChanged)   onSlotsChanged();
    });
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
        slots.push_back (std::move (s));
    }
}

void MementoEngine::addSlot (Role role)
{
    {
        const ScopedLock sl (slotsLock);
        auto s = std::make_unique<Slot>();
        s->role = role;
        slots.push_back (std::move (s));
    }
    rerollSlot ((int) slots.size() - 1);
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

// --- sélection -------------------------------------------------------------
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
    // Pour les rôles superposés, on privilégie les loops calables proprement.
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

void MementoEngine::rerollSlot (int index)
{
    Slot* s = getSlot (index);
    if (s == nullptr) return;
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

        // scan en attente ?
        File toScan;
        { const ScopedLock sl (jobLock); toScan = pendingScan; pendingScan = File(); }
        if (toScan.isDirectory()) { doScan (toScan); lastRenderedBpm = hostBpm.load(); }

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
    int recIndex = -1; Role role = Role::Drums;
    {
        const ScopedLock sl (slotsLock);
        if (slotIndex < 0 || slotIndex >= (int) slots.size()) return;
        recIndex = slots[(size_t) slotIndex]->recordIndex;
        role     = slots[(size_t) slotIndex]->role;
    }
    if (recIndex < 0) return;

    Record rec;
    { const ScopedLock sl (dataLock); if (recIndex >= (int) records.size()) return; rec = records[(size_t) recIndex]; }

    auto clip = makeClip (rec);

    // Stocke le clip dans le slot (s'il existe toujours et pointe le même record).
    {
        const ScopedLock sl (slotsLock);
        if (slotIndex < (int) slots.size() && slots[(size_t) slotIndex]->recordIndex == recIndex)
        {
            std::atomic_store (&slots[(size_t) slotIndex]->clip, clip);
            slots[(size_t) slotIndex]->rendering.store (false);
        }
    }
    MessageManager::callAsync ([this] { if (onSlotsChanged) onSlotsChanged(); });
}

// --- rendu offline : chargement + stretch + resample -----------------------
std::shared_ptr<StretchedClip> MementoEngine::makeClip (const Record& rec)
{
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (rec.file));
    if (reader == nullptr) return nullptr;

    const int fileCh = (int) juce::jlimit (1, 2, (int) reader->numChannels);
    const double fileSr = reader->sampleRate > 0 ? reader->sampleRate : sr;
    const int64 maxFrames = (int64) (fileSr * 16.0); // borne à 16 s
    const int64 frames = juce::jmin ((int64) reader->lengthInSamples, maxFrames);
    if (frames < 256) return nullptr;

    AudioBuffer<float> src ((int) reader->numChannels, (int) frames);
    reader->read (&src, 0, (int) frames, 0, true, true);

    const double srcBpm = rec.bpm > 0 ? (double) rec.bpm : 0.0;
    const double bpm    = hostBpm.load();
    const double tempoRatio = (srcBpm > 0.0) ? juce::jlimit (0.25, 4.0, bpm / srcBpm) : 1.0;

    // --- SoundTouch (interleaved, au SR du fichier) ---
    soundtouch::SoundTouch st;
    st.setSampleRate ((unsigned) fileSr);
    st.setChannels ((unsigned) fileCh);
    st.setTempo (tempoRatio);
    st.setSetting (SETTING_USE_QUICKSEEK, 0);
    st.setSetting (SETTING_USE_AA_FILTER, 1);

    std::vector<float> interleaved ((size_t) frames * (size_t) fileCh);
    for (int64 i = 0; i < frames; ++i)
        for (int c = 0; c < fileCh; ++c)
            interleaved[(size_t) (i * fileCh + c)] = src.getSample (juce::jmin (c, src.getNumChannels() - 1), (int) i);

    std::vector<float> outBuf;
    outBuf.reserve ((size_t) ((double) frames / tempoRatio + 4096.0) * (size_t) fileCh);
    const unsigned block = 4096;
    std::vector<float> recv ((size_t) block * (size_t) fileCh);

    int64 fed = 0;
    while (fed < frames)
    {
        unsigned n = (unsigned) juce::jmin ((int64) block, frames - fed);
        st.putSamples (&interleaved[(size_t) (fed * fileCh)], n);
        fed += n;
        unsigned got;
        while ((got = st.receiveSamples (recv.data(), block)) > 0)
            outBuf.insert (outBuf.end(), recv.begin(), recv.begin() + (size_t) got * fileCh);
    }
    st.flush();
    unsigned got;
    while ((got = st.receiveSamples (recv.data(), block)) > 0)
        outBuf.insert (outBuf.end(), recv.begin(), recv.begin() + (size_t) got * fileCh);

    const int64 stFrames = (int64) (outBuf.size() / (size_t) fileCh);
    if (stFrames < 64) return nullptr;

    // --- resample fileSr -> hostSr (linéaire), sortie stéréo ---
    const double ratio = sr / fileSr;
    const int64 outFrames = (int64) std::floor (stFrames * ratio);
    auto clip = std::make_shared<StretchedClip>();
    clip->buffer.setSize (2, (int) outFrames, false, true, true);
    for (int64 i = 0; i < outFrames; ++i)
    {
        double srcPos = (double) i / ratio;
        int64 i0 = (int64) srcPos;
        int64 i1 = juce::jmin (i0 + 1, stFrames - 1);
        float frac = (float) (srcPos - (double) i0);
        for (int c = 0; c < 2; ++c)
        {
            int sc = juce::jmin (c, fileCh - 1);
            float a = outBuf[(size_t) (i0 * fileCh + sc)];
            float b = outBuf[(size_t) (i1 * fileCh + sc)];
            clip->buffer.setSample (c, (int) i, a + (b - a) * frac);
        }
    }

    // --- longueur de boucle calée sur des mesures entières ---
    int64 loopLen = outFrames;
    if (srcBpm > 0.0)
    {
        double srcDurSec = (double) frames / fileSr;
        int beats = snapBeats (srcDurSec * srcBpm / 60.0);
        loopLen = (int64) std::llround ((double) beats * (60.0 / bpm) * sr);
    }
    clip->loopLen = juce::jlimit ((int64) 64, outFrames, loopLen);
    clip->name = rec.name;
    clip->sourceBpm = rec.bpm;
    return clip;
}

// --- lecture (thread audio) ------------------------------------------------
void MementoEngine::process (AudioBuffer<float>& out, int64 hostPosSamples, bool playing)
{
    out.clear();
    if (! playing) return;

    const ScopedTryLock stl (slotsLock);
    if (! stl.isLocked()) return; // structure en cours de modif → un bloc de silence

    // y a-t-il un solo actif ?
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

        const float g = s->gain.load();
        const float pan = s->pan.load();
        const float gl = g * std::sqrt (0.5f * (1.0f - pan));
        const float gr = g * std::sqrt (0.5f * (1.0f + pan));
        const int64 loopLen = clip->loopLen;
        const int clipCh = clip->buffer.getNumChannels();
        const int clipN = clip->buffer.getNumSamples();
        const float* cl = clip->buffer.getReadPointer (0);
        const float* cr = clip->buffer.getReadPointer (clipCh > 1 ? 1 : 0);
        float* ol = out.getWritePointer (0);
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
    Array<var> arr;
    {
        const ScopedLock sl (slotsLock);
        for (auto& sp : slots)
        {
            var o (new DynamicObject());
            o.getDynamicObject()->setProperty ("role", (int) sp->role);
            o.getDynamicObject()->setProperty ("gain", (double) sp->gain.load());
            o.getDynamicObject()->setProperty ("pan",  (double) sp->pan.load());
            o.getDynamicObject()->setProperty ("mute", sp->mute.load());
            o.getDynamicObject()->setProperty ("solo", sp->solo.load());
            o.getDynamicObject()->setProperty ("lock", sp->locked.load());
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

    if (auto* arr = root.getProperty ("slots", var()).getArray())
    {
        const ScopedLock sl (slotsLock);
        slots.clear();
        for (auto& v : *arr)
        {
            auto sp = std::make_unique<Slot>();
            sp->role = (Role) (int) v.getProperty ("role", (int) Role::Tonal);
            sp->gain.store ((float) (double) v.getProperty ("gain", 0.85));
            sp->pan.store  ((float) (double) v.getProperty ("pan", 0.0));
            sp->mute.store ((bool) v.getProperty ("mute", false));
            sp->solo.store ((bool) v.getProperty ("solo", false));
            sp->locked.store ((bool) v.getProperty ("lock", false));
            slots.push_back (std::move (sp));
        }
    }
    if (folder.isDirectory()) scanFolder (folder); // relance scan + première combinaison
}

} // namespace mem

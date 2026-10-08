#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <algorithm>
#if JUCE_MAC || JUCE_IOS
 #include <Accelerate/Accelerate.h>
#endif

// ============================================================================
//  Velours — moteur de suppression dynamique des résonances (v1.0)
//
//  Principe (même famille que soothe3) :
//   1. STFT (fenêtre de Hann, recouvrement 4x à 16x, une trame toutes les ~5 ms) ;
//   2. spectre de détection lissé à la résolution choisie par DETAIL
//      (fin = coupes étroites et profondes, large = réduction des accumulations),
//      puis lissé dans le temps (plus lent en détail élevé, comme soothe3) ;
//   3. seuil adaptatif (mode SOFT) : on relie les sommets du spectre et on en
//      tire une enveloppe de référence robuste. Ce qui dépasse ses voisins est
//      une résonance ; le traitement ne dépend pas du niveau d'entrée ;
//   4. mode HARD : dépend du niveau absolu (seuil fixe), réagit en plus aux
//      montées soudaines -> plus ferme, proche d'un compresseur multibande ;
//   5. atténuation = excès x DEPTH, pondérée par la courbe de profondeur
//      (éditeur de bandes), plafonnée par MAX CUT, lissée en fréquence ;
//   6. attaque / relâchement par case de fréquence, naturellement plus rapides
//      dans l'aigu, modulés par ATTACK TILT et RELEASE TILT ;
//   7. suivi des résonances : les creux les plus marqués sont publiés pour
//      l'écran (fréquence, profondeur) à chaque trame ;
//   8. BAND LISTEN : on n'entend que ce qui est retiré dans la zone d'une bande ;
//   9. ZERO LATENCY : la même analyse pilote un filtre à phase minimale
//      (calculé par cepstre à chaque trame) appliqué sans retard ;
//  10. GAIN MATCH : compense la baisse de niveau (mesure pondérée rose, lente).
// ============================================================================
namespace velours
{
struct Settings
{
    float depth = 5.0f;          // 0..20
    float detail = 0.5f;         // 0..1
    float attackMs = 5.0f, releaseMs = 50.0f;
    float maxCutDb = 30.0f;
    bool hard = false;
    float detailTilt = 0.0f;     // -1..1
    float attackTilt = 0.0f;     // -1..1
    float releaseTilt = 0.0f;    // -1..1
    bool midSide = false;
    float link = 1.0f;           // 0..1
    bool sidechain = false;
    float mix = 1.0f;            // 0..1
    float wetTrimDb = 0.0f, outputDb = 0.0f;
    bool delta = false, bypass = false;
    bool listen = false;         // écoute de la zone d'une bande (delta filtré)
    float focus = 0.0f;          // -1 = tout sur L/M, +1 = tout sur R/S
    bool gainMatch = false;      // compensation automatique du niveau
};

class Engine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int numQualities = 3;   // tailles FFT (Low Latency, Normal, High Res)
    static constexpr int zeroLatencyIndex = 3; // RESOLUTION « Zero Latency »
    static constexpr int maxFir = 2048;
    static constexpr int maxTracked = 6;

    // --------------------------------------------------------------- setup
    void prepare (double sampleRate, int initialQuality, int initialTimeQuality = 0)
    {
        sr = sampleRate;
        scale = sr <= 50000.0 ? 1 : (sr <= 100000.0 ? 2 : 4);
        const int scaleOrder = scale == 1 ? 0 : (scale == 2 ? 1 : 2);
        for (int q = 0; q < numQualities; ++q)
            ffts[(size_t) q] = std::make_unique<juce::dsp::FFT> (10 + q + scaleOrder);

        nMax = 4096 * scale;
        const int binsMax = nMax / 2 + 1;
        for (int c = 0; c < maxChannels; ++c)
        {
            inRing[c].assign ((size_t) nMax, 0.0f);
            scRing[c].assign ((size_t) nMax, 0.0f);
            dryRing[c].assign ((size_t) nMax, 0.0f);
            outAcc[c].assign ((size_t) nMax, 0.0f);
            outQueue[c].assign ((size_t) nMax, 0.0f);
            spec[c].assign ((size_t) nMax * 2, 0.0f);
            scSpec[c].assign ((size_t) nMax * 2, 0.0f);
            power[c].assign ((size_t) binsMax, 0.0f);
            scPower[c].assign ((size_t) binsMax, 0.0f);
            target[c].assign ((size_t) binsMax, 0.0f);
            red[c].assign ((size_t) binsMax, 0.0f);
            weight[c].assign ((size_t) binsMax, 1.0f);
        }
        for (int p = 0; p < 3; ++p)
        {
            fineDb[p].assign ((size_t) binsMax, -200.0f);
            slowDb[p].assign ((size_t) binsMax, -200.0f);
            detPow[p].assign ((size_t) binsMax, 0.0f);
            pathT[p].assign ((size_t) binsMax, 0.0f);
        }
        tmpA.assign ((size_t) binsMax, 0.0f);
        tmpB.assign ((size_t) binsMax, 0.0f);
        tmpP.assign ((size_t) binsMax, 0.0f);
        prefix.assign ((size_t) binsMax + 1, 0.0);
        fineLo.assign ((size_t) binsMax, 0); fineHi.assign ((size_t) binsMax, 0);
        envLo.assign ((size_t) binsMax, 0);  envHi.assign ((size_t) binsMax, 0);
        maskLo.assign ((size_t) binsMax, 0); maskHi.assign ((size_t) binsMax, 0);
        thrDb.assign ((size_t) binsMax, 0.0f);
        coefAtt.assign ((size_t) binsMax, 0.0f);
        coefRel.assign ((size_t) binsMax, 0.0f);
        coefDetUp.assign ((size_t) binsMax, 0.0f);
        coefDetDown.assign ((size_t) binsMax, 0.0f);
        pinkDb.assign ((size_t) binsMax, 0.0f);
        listenMask.assign ((size_t) binsMax, 0.0f);
        window.assign ((size_t) nMax, 0.0f);
        linkPower.assign ((size_t) binsMax, 0.0f);
        weightW.assign ((size_t) binsMax, 0.0f);
        cep.assign ((size_t) nMax * 2, 0.0f);
        for (int c = 0; c < maxChannels; ++c)
        {
            firCur[c].assign ((size_t) maxFir, 0.0f);
            firPrev[c].assign ((size_t) maxFir, 0.0f);
            hist[c].assign ((size_t) (maxFir + 256 * scale), 0.0f);
            wetCur[c].assign ((size_t) (256 * scale), 0.0f);
            wetPrev[c].assign ((size_t) (256 * scale), 0.0f);
        }

        {
            // l'écran lit ces tableaux sur un autre fil : jamais de réallocation hors verrou
            const juce::SpinLock::ScopedLockType sl (ui.lock);
            ui.bins = 0;
            ui.numTracked = 0;
            ui.inDb.assign ((size_t) binsMax, -200.0f);
            ui.outDb.assign ((size_t) binsMax, -200.0f);
            ui.redDb.assign ((size_t) binsMax, 0.0f);
        }

        for (auto* s : { &wetTrimSm, &outputSm, &mixSm, &bypassSm, &deltaSm, &listenSm, &makeupSm })
            s->reset (sr, 0.03);

        setQuality (initialQuality, initialTimeQuality);
    }

    // Résolution (taille FFT = latence) et qualité temporelle (pas entre trames :
    // Normal 5,3 ms, High 2,7 ms, Ultra 1,3 ms à 48 kHz — la latence ne change pas).
    // Sans allocation ; remet l'état à zéro.
    void setQuality (int q, int tq)
    {
        zeroLatency = q == zeroLatencyIndex;
        quality = zeroLatency ? 1 : juce::jlimit (0, numQualities - 1, q);   // l'analyse reste en « Normal »
        timeQuality = juce::jlimit (0, 2, tq);
        N = (1024 << quality) * scale;
        hop = (256 >> timeQuality) * scale;
        bins = N / 2 + 1;
        fft = ffts[(size_t) quality].get();
        binHz = (float) (sr / N);
        frameSeconds = (float) hop / (float) sr;

        for (int i = 0; i < N; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);
        const float overlap = (float) N / (float) hop;
        olaGain = 8.0f / (3.0f * overlap);                 // somme des Hann² = 3R/8
        fsNorm = 20.0f * std::log10 ((float) N / 4.0f);    // sinus pleine échelle -> 0 dB
        levelNorm = 5.0f * std::log10 ((float) N / 2048.0f); // niveau comparable d'une résolution à l'autre

        for (int k = 0; k < bins; ++k)
        {
            pinkDb[(size_t) k] = 3.0f * std::log2 (std::max (20.0f, (float) k * binHz) / 1000.0f);
            weightW[(size_t) k] = loudnessWeight ((float) k * binHz);
        }
        firLen = std::min (maxFir, N / 2);

        tablesValid = false;
        reset();
    }

    void reset()
    {
        for (int c = 0; c < maxChannels; ++c)
        {
            std::fill (inRing[c].begin(), inRing[c].end(), 0.0f);
            std::fill (scRing[c].begin(), scRing[c].end(), 0.0f);
            std::fill (dryRing[c].begin(), dryRing[c].end(), 0.0f);
            std::fill (outAcc[c].begin(), outAcc[c].end(), 0.0f);
            std::fill (outQueue[c].begin(), outQueue[c].end(), 0.0f);
            std::fill (red[c].begin(), red[c].end(), 0.0f);
        }
        for (int p = 0; p < 3; ++p)
        {
            std::fill (slowDb[p].begin(), slowDb[p].end(), -200.0f);
            std::fill (detPow[p].begin(), detPow[p].end(), 0.0f);
        }
        for (int c = 0; c < maxChannels; ++c)
        {
            std::fill (firCur[c].begin(), firCur[c].end(), 0.0f);
            std::fill (firPrev[c].begin(), firPrev[c].end(), 0.0f);
            firCur[c][0] = firPrev[c][0] = 1.0f;     // filtre neutre
            curIdentity[c] = prevIdentity[c] = true;
            std::fill (hist[c].begin(), hist[c].end(), 0.0f);
        }
        ringPos = 0;
        hopCount = 0;
        dryPos = 0;
        firstFrame = true;
        makeupDb = 0.0f;
        loudIn = loudOut = 0.0;
        makeupSm.setCurrentAndTargetValue (1.0f);
    }

    int getLatency() const noexcept   { return zeroLatency ? 0 : N; }
    bool isZeroLatency() const noexcept { return zeroLatency; }
    int getNumBins() const noexcept   { return bins; }
    float getBinHz() const noexcept   { return binHz; }
    int getQuality() const noexcept   { return zeroLatency ? zeroLatencyIndex : quality; }
    int getTimeQuality() const noexcept { return timeQuality; }

    // Multiplicateur de profondeur par case (éditeur de bandes), canal de traitement c
    float* getWeights (int c) noexcept { return weight[c].data(); }
    // Zone écoutée en BAND LISTEN (0..1 par case)
    float* getListenMask() noexcept { return listenMask.data(); }

    // --------------------------------------------------------------- audio
    void process (float* const* io, int numCh, const float* const* sc, int scCh, int n, const Settings& s)
    {
        numCh = juce::jlimit (1, maxChannels, numCh);
        procCh = numCh;
        settings = s;
        useSc = s.sidechain && sc != nullptr && scCh > 0;
        msActive = s.midSide && numCh == 2;

        if (! tablesValid || tablesChanged (s))
            updateTables (s);

        wetTrimSm.setTargetValue (juce::Decibels::decibelsToGain (s.wetTrimDb));
        outputSm.setTargetValue (juce::Decibels::decibelsToGain (s.outputDb));
        mixSm.setTargetValue (s.mix);
        bypassSm.setTargetValue (s.bypass ? 1.0f : 0.0f);
        deltaSm.setTargetValue (s.delta ? 1.0f : 0.0f);
        listenSm.setTargetValue (s.listen ? 1.0f : 0.0f);
        makeupSm.setTargetValue (s.gainMatch && ! s.delta ? juce::Decibels::decibelsToGain (makeupDb) : 1.0f);

        if (zeroLatency)
        {
            processZeroLatency (io, numCh, sc, scCh, n);
            return;
        }

        const int mask = N - 1;
        int done = 0;
        while (done < n)
        {
            const int todo = std::min (n - done, hop - hopCount);
            for (int i = 0; i < todo; ++i)
            {
                const int idx = done + i;
                float x[maxChannels] {}, k[maxChannels] {};
                for (int c = 0; c < numCh; ++c) x[c] = io[c][idx];

                if (useSc)
                {
                    if (scCh >= numCh) for (int c = 0; c < numCh; ++c) k[c] = sc[c][idx];
                    else { const float m = sc[0][idx]; for (int c = 0; c < numCh; ++c) k[c] = m; }
                }

                // Entrée (M/S éventuel) -> anneau d'analyse
                if (msActive)
                {
                    inRing[0][(size_t) ringPos] = 0.5f * (x[0] + x[1]);
                    inRing[1][(size_t) ringPos] = 0.5f * (x[0] - x[1]);
                    if (useSc && scCh < 2)
                    {
                        scRing[0][(size_t) ringPos] = k[0];
                        scRing[1][(size_t) ringPos] = k[0];
                    }
                    else if (useSc)
                    {
                        scRing[0][(size_t) ringPos] = 0.5f * (k[0] + k[1]);
                        scRing[1][(size_t) ringPos] = 0.5f * (k[0] - k[1]);
                    }
                }
                else
                {
                    for (int c = 0; c < numCh; ++c)
                    {
                        inRing[c][(size_t) ringPos] = x[c];
                        if (useSc) scRing[c][(size_t) ringPos] = k[c];
                    }
                }
                ringPos = (ringPos + 1) & mask;

                // Sortie traitée (retard = N)
                float w[maxChannels] {};
                for (int c = 0; c < numCh; ++c) w[c] = outQueue[c][(size_t) (hopCount + i)];
                if (msActive) { const float m = w[0], sd = w[1]; w[0] = m + sd; w[1] = m - sd; }

                // Signal sec retardé d'exactement N échantillons
                float d[maxChannels] {};
                for (int c = 0; c < numCh; ++c)
                {
                    d[c] = dryRing[c][(size_t) dryPos];
                    dryRing[c][(size_t) dryPos] = x[c];
                }
                dryPos = (dryPos + 1) & mask;

                const float trim = wetTrimSm.getNextValue();
                const float mx   = mixSm.getNextValue();
                const float og   = outputSm.getNextValue();
                const float bp   = bypassSm.getNextValue();
                const float dl   = deltaSm.getNextValue();
                const float ls   = listenSm.getNextValue();
                const float mk   = makeupSm.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    io[c][idx] = mixSample (d[c], w[c], trim, mx, og, bp, dl, ls, mk);
            }
            hopCount += todo;
            done += todo;
            if (hopCount == hop)
            {
                processFrame();
                hopCount = 0;
            }
        }
    }

    // Pondération d'écoute (approximation de la pondération K des LUFS, ITU-R BS.1770) :
    // passe-haut ~38 Hz + plateau +4 dB au-dessus de ~1,5 kHz. Valeur en puissance.
    static float loudnessWeight (float f) noexcept
    {
        if (f <= 0.0f) return 0.0f;
        const float f4 = f * f * f * f, c4 = 38.0f * 38.0f * 38.0f * 38.0f;
        const float hp = f4 / (f4 + c4);
        const float shelf = 1.0f + 1.5119f * (f * f) / (f * f + 1500.0f * 1500.0f);   // 10^(4/10) - 1 = 1,5119
        return hp * shelf;
    }

    // Sortie : sec / traité, trim, delta, écoute de bande, bypass, compensation
    static inline float mixSample (float d, float w, float trim, float mx, float og, float bp, float dl, float ls, float mk) noexcept
    {
        const float out = d + mx * (w * trim - d);               // ce que le traitement produit
        const float delta = d - out;                             // ce qui est retiré
        float y = (out + mx * w * trim * (mk - 1.0f)) * og;      // + compensation de niveau
        y += dl * (delta * og - y);
        y += ls * (w * og - y);                                  // BAND LISTEN : la sortie traitée EST le delta filtré
        return y + bp * (d - y);
    }

    // ---------------------------------------------------------- zéro latence
    // y[i] = somme h[j] x[L-1+i-j] : x pointe sur l'historique (L-1 échantillons passés puis le bloc)
    static void convolve (const float* x, const float* h, int L, float* y, int n) noexcept
    {
       #if JUCE_MAC || JUCE_IOS
        vDSP_conv (x, 1, h + L - 1, -1, y, 1, (vDSP_Length) n, (vDSP_Length) L);
       #else
        for (int i = 0; i < n; ++i)
        {
            const float* xp = x + i + L - 1;
            float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
            int j = 0;
            for (; j + 3 < L; j += 4)
            {
                a0 += h[j] * xp[-j];
                a1 += h[j + 1] * xp[-j - 1];
                a2 += h[j + 2] * xp[-j - 2];
                a3 += h[j + 3] * xp[-j - 3];
            }
            for (; j < L; ++j) a0 += h[j] * xp[-j];
            y[i] = (a0 + a1) + (a2 + a3);
        }
       #endif
    }

    void processZeroLatency (float* const* io, int numCh, const float* const* sc, int scCh, int n)
    {
        const int mask = N - 1;
        const int L = firLen;
        int done = 0;
        while (done < n)
        {
            const int todo = std::min (n - done, hop - hopCount);

            // 1. entrées -> anneau d'analyse + historique de convolution
            for (int i = 0; i < todo; ++i)
            {
                const int idx = done + i;
                float x[maxChannels] {}, k[maxChannels] {}, e[maxChannels] {};
                for (int c = 0; c < numCh; ++c) x[c] = io[c][idx];
                if (useSc)
                {
                    if (scCh >= numCh) for (int c = 0; c < numCh; ++c) k[c] = sc[c][idx];
                    else { const float m = sc[0][idx]; for (int c = 0; c < numCh; ++c) k[c] = m; }
                }
                if (msActive)
                {
                    e[0] = 0.5f * (x[0] + x[1]);
                    e[1] = 0.5f * (x[0] - x[1]);
                    if (useSc)
                    {
                        scRing[0][(size_t) ringPos] = scCh < 2 ? k[0] : 0.5f * (k[0] + k[1]);
                        scRing[1][(size_t) ringPos] = scCh < 2 ? k[0] : 0.5f * (k[0] - k[1]);
                    }
                }
                else
                {
                    for (int c = 0; c < numCh; ++c)
                    {
                        e[c] = x[c];
                        if (useSc) scRing[c][(size_t) ringPos] = k[c];
                    }
                }
                for (int c = 0; c < numCh; ++c)
                {
                    inRing[c][(size_t) ringPos] = e[c];
                    hist[c][(size_t) (L - 1 + hopCount + i)] = e[c];
                }
                ringPos = (ringPos + 1) & mask;
            }

            // 2. filtrage sans retard (fondu entre le filtre précédent et le nouveau sur la trame)
            for (int c = 0; c < numCh; ++c)
            {
                const float* x = hist[c].data() + hopCount;
                float* wc = wetCur[c].data();
                float* wp = wetPrev[c].data();
                if (curIdentity[c]) std::copy (x + L - 1, x + L - 1 + todo, wc);
                else convolve (x, firCur[c].data(), L, wc, todo);
                if (prevIdentity[c]) std::copy (x + L - 1, x + L - 1 + todo, wp);
                else convolve (x, firPrev[c].data(), L, wp, todo);
                const float inv = 1.0f / (float) hop;
                for (int i = 0; i < todo; ++i)
                {
                    const float t = (float) (hopCount + i + 1) * inv;
                    wc[i] = wp[i] + t * (wc[i] - wp[i]);
                }
            }

            // 3. mélange (sec = entrée, sans retard)
            for (int i = 0; i < todo; ++i)
            {
                const int idx = done + i;
                float w[maxChannels] {};
                for (int c = 0; c < numCh; ++c) w[c] = wetCur[c][(size_t) i];
                if (msActive) { const float m = w[0], sd = w[1]; w[0] = m + sd; w[1] = m - sd; }
                const float trim = wetTrimSm.getNextValue();
                const float mx   = mixSm.getNextValue();
                const float og   = outputSm.getNextValue();
                const float bp   = bypassSm.getNextValue();
                const float dl   = deltaSm.getNextValue();
                const float ls   = listenSm.getNextValue();
                const float mk   = makeupSm.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    io[c][idx] = mixSample (io[c][idx], w[c], trim, mx, og, bp, dl, ls, mk);
            }

            hopCount += todo;
            done += todo;
            if (hopCount == hop)
            {
                processFrame();
                hopCount = 0;
                for (int c = 0; c < numCh; ++c)   // garde les L-1 derniers échantillons
                    std::memmove (hist[c].data(), hist[c].data() + hop, sizeof (float) * (size_t) (L - 1));
            }
        }
    }

    // Filtre à phase minimale de module |H| = gains (cepstre replié), tronqué à firLen points
    void designMinPhase (const float* logMag, float* h)
    {
        float* cb = cep.data();
        for (int k = 0; k < bins; ++k) { cb[2 * k] = logMag[k]; cb[2 * k + 1] = 0.0f; }
        std::fill (cb + 2 * bins, cb + 2 * N, 0.0f);
        fft->performRealOnlyInverseTransform (cb);                 // cepstre réel (pair)
        const int half = N / 2;
        for (int i = 1; i < half; ++i) cb[i] *= 2.0f;              // repli causal
        std::fill (cb + half + 1, cb + 2 * N, 0.0f);
        fft->performRealOnlyForwardTransform (cb, true);
        for (int k = 0; k < bins; ++k)
        {
            const float m = std::exp (cb[2 * k]);
            const float ph = cb[2 * k + 1];
            cb[2 * k] = m * std::cos (ph);
            cb[2 * k + 1] = m * std::sin (ph);
        }
        fft->performRealOnlyInverseTransform (cb);                 // réponse impulsionnelle
        const int L = firLen, taper = L / 4;
        for (int i = 0; i < L; ++i)
        {
            float w = 1.0f;
            if (i >= L - taper)
                w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (i - (L - taper)) / (float) taper);
            h[i] = cb[i] * w;
        }
    }

    // --------------------------------------------------------------- interface
    struct UiData
    {
        juce::SpinLock lock;
        std::vector<float> inDb, outDb, redDb;   // dBFS / dB de réduction
        int bins = 0;
        float binHz = 1.0f;
        int counter = 0;
        // Résonances suivies (les creux les plus profonds de la trame)
        int numTracked = 0;
        std::array<float, maxTracked> trackedHz {}, trackedDb {};
    };
    UiData ui;
    std::atomic<float> overallReductionDb { 0.0f };   // baisse de niveau globale due au traitement
    std::atomic<float> peakReductionDb { 0.0f };      // plus forte coupure en cours
    std::atomic<float> makeupDbOut { 0.0f };           // compensation GAIN MATCH estimée

private:
    // ----------------------------------------------------------- tables par case
    bool tablesChanged (const Settings& s) const
    {
        using juce::exactlyEqual;
        return ! exactlyEqual (s.detail, tDetail) || ! exactlyEqual (s.detailTilt, tDetailTilt)
            || ! exactlyEqual (s.attackMs, tAttack) || ! exactlyEqual (s.releaseMs, tRelease)
            || ! exactlyEqual (s.attackTilt, tAttackTilt) || ! exactlyEqual (s.releaseTilt, tReleaseTilt)
            || s.hard != tHard;
    }

    static void widthToRange (int k, float halfOct, int minHalf, int bins, int& lo, int& hi)
    {
        const float f = std::max (1.0f, (float) k);
        int l = (int) std::floor (f * std::exp2 (-halfOct));
        int h = (int) std::ceil  (f * std::exp2 ( halfOct));
        l = std::min (l, k - minHalf);
        h = std::max (h, k + minHalf);
        lo = juce::jlimit (1, bins - 1, l);
        hi = juce::jlimit (1, bins - 1, h);
    }

    void updateTables (const Settings& s)
    {
        tDetail = s.detail; tDetailTilt = s.detailTilt; tAttack = s.attackMs;
        tRelease = s.releaseMs; tAttackTilt = s.attackTilt; tReleaseTilt = s.releaseTilt; tHard = s.hard;

        for (int k = 0; k < bins; ++k)
        {
            const float f = std::max (binHz, (float) k * binHz);
            const float octFrom1k = std::log2 (f / 1000.0f);
            const float d = juce::jlimit (0.0f, 1.0f, s.detail + s.detailTilt * 0.15f * octFrom1k);

            // Résolution de détection : 1/3 oct (accumulations) -> 1/36 oct (chirurgical)
            const float fineOct = (1.0f / 3.0f) * std::pow ((1.0f / 36.0f) / (1.0f / 3.0f), d);
            // Enveloppe de référence : 4 oct -> 0,75 oct
            const float envOct  = 4.0f * std::pow (0.75f / 4.0f, d);
            // Largeur des coupes
            const float maskOct = fineOct * 0.75f;

            widthToRange (k, fineOct * 0.5f, 1, bins, fineLo[(size_t) k], fineHi[(size_t) k]);
            widthToRange (k, envOct * 0.5f, 5, bins, envLo[(size_t) k], envHi[(size_t) k]);
            widthToRange (k, maskOct * 0.5f, 1, bins, maskLo[(size_t) k], maskHi[(size_t) k]);

            // Sélectivité : plus de détail = seuls les pics marqués sont traités
            thrDb[(size_t) k] = 0.5f + 1.5f * d;

            // Lissage temporel de la détection : plus lent en détail élevé (soothe3)
            const float tDet = (3.0f + 15.0f * d) * 0.001f;
            coefDetUp[(size_t) k]   = std::exp (-frameSeconds / (tDet * 0.35f));
            coefDetDown[(size_t) k] = std::exp (-frameSeconds / tDet);

            // Temps : naturellement plus rapides dans l'aigu, puis TILT
            const float base = juce::jlimit (0.35f, 3.0f, std::pow (1000.0f / f, 0.3f));
            const float tfA = base * juce::jlimit (0.125f, 8.0f, std::exp2 (-s.attackTilt * 0.6f * octFrom1k));
            const float tfR = base * juce::jlimit (0.125f, 8.0f, std::exp2 (-s.releaseTilt * 0.6f * octFrom1k));
            const float att = std::max (0.05f, s.attackMs * tfA) * 0.001f;
            const float rel = std::max (0.5f, s.releaseMs * tfR) * 0.001f;
            coefAtt[(size_t) k] = std::exp (-frameSeconds / att);
            coefRel[(size_t) k] = std::exp (-frameSeconds / rel);
        }
        slowCoef = std::exp (-frameSeconds / 0.25f);
        tablesValid = true;
    }

    // Moyenne glissante à largeur variable (sommes cumulées)
    void boxAverage (const float* src, float* dst, const std::vector<int>& lo, const std::vector<int>& hi)
    {
        prefix[0] = 0.0;
        for (int k = 0; k < bins; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) src[k];
        for (int k = 0; k < bins; ++k)
        {
            const int l = lo[(size_t) k], h = hi[(size_t) k];
            dst[k] = (float) ((prefix[(size_t) h + 1] - prefix[(size_t) l]) / (double) (h - l + 1));
        }
    }

    // Interpolation linéaire entre les maxima locaux (seuil adaptatif)
    void upperHull (const float* src, float* dst) const
    {
        int prevK = -1;
        float prevV = 0.0f;
        for (int k = 1; k < bins; ++k)
        {
            const bool isPeak = src[k] >= src[k - 1] && (k == bins - 1 || src[k] >= src[k + 1]);
            if (! isPeak) continue;
            if (prevK < 0)
            {
                for (int j = 0; j <= k; ++j) dst[j] = src[k];
            }
            else
            {
                const float step = (src[k] - prevV) / (float) (k - prevK);
                for (int j = prevK + 1; j <= k; ++j) dst[j] = prevV + step * (float) (j - prevK);
            }
            prevK = k;
            prevV = src[k];
        }
        if (prevK < 0) { std::copy (src, src + bins, dst); return; }
        for (int j = prevK + 1; j < bins; ++j) dst[j] = prevV;
    }

    // Calcule la réduction voulue (dB) pour un chemin de détection
    void computeTargets (const float* pw, int path, float* out)
    {
        float* fine = fineDb[path].data();
        float* slow = slowDb[path].data();
        float* dp   = detPow[path].data();

        // 1. spectre de détection : lissé en fréquence (DETAIL) puis dans le temps
        boxAverage (pw, tmpP.data(), fineLo, fineHi);
        for (int k = 0; k < bins; ++k)
        {
            const float p = tmpP[(size_t) k];
            if (firstFrame) dp[k] = p;
            else            dp[k] = p + (p > dp[k] ? coefDetUp[(size_t) k] : coefDetDown[(size_t) k]) * (dp[k] - p);
            fine[k] = 10.0f * std::log10 (dp[k] + 1.0e-24f) - fsNorm;
        }

        // 2. enveloppe « haute » : on relie les sommets (les harmoniques normales
        //    d'une voix ou d'un synthé ne sont pas des résonances)
        upperHull (fine, tmpB.data());

        // 3. enveloppe de référence robuste : 2 passes, pics écrêtés à +3 dB
        boxAverage (tmpB.data(), tmpA.data(), envLo, envHi);
        for (int k = 0; k < bins; ++k) tmpB[(size_t) k] = std::min (tmpB[(size_t) k], tmpA[(size_t) k] + 3.0f);
        boxAverage (tmpB.data(), tmpA.data(), envLo, envHi);

        const bool hard = settings.hard;
        const float knee = hard ? 1.5f : 8.0f;
        const float depthScale = settings.depth / 6.0f;
        constexpr float hardRef = -45.0f;   // niveau « fort » de référence (par case, pondéré rose)

        for (int k = 0; k < bins; ++k)
        {
            float x = fine[k] - tmpA[(size_t) k] - thrDb[(size_t) k];
            float levelFactor = 1.0f;

            if (hard)
            {
                const float level = fine[k] + pinkDb[(size_t) k] + levelNorm;
                // Seuil fixe : ce qui est fort en absolu est réduit davantage
                levelFactor = juce::jlimit (0.3f, 2.0f, 1.0f + (level - hardRef) / 24.0f);
                x = std::max (x, 0.5f * (level - (hardRef + 12.0f)));
                // Montée soudaine par rapport au passé récent
                if (firstFrame || slow[k] < -150.0f) slow[k] = fine[k];
                x = std::max (x, fine[k] - slow[k] - 4.0f);
                slow[k] = fine[k] + slowCoef * (slow[k] - fine[k]);
            }

            float y;
            if (x <= -0.5f * knee)      y = 0.0f;
            else if (x >= 0.5f * knee)  y = x;
            else                        { const float t = x + 0.5f * knee; y = t * t / (2.0f * knee); }

            // Rien sous le plancher de bruit
            const float gate = juce::jlimit (0.0f, 1.0f, (fine[k] + 110.0f) * 0.1f);
            out[k] = depthScale * y * levelFactor * gate;
        }
        out[0] = 0.0f;
    }

    void processFrame()
    {
        const int mask = N - 1;
        const int nc = procCh;

        // --- analyse
        for (int c = 0; c < nc; ++c)
        {
            float* sp = spec[c].data();
            for (int j = 0; j < N; ++j)
                sp[j] = inRing[c][(size_t) ((ringPos + j) & mask)] * window[(size_t) j];
            std::fill (sp + N, sp + 2 * N, 0.0f);
            fft->performRealOnlyForwardTransform (sp, true);
            float* pw = power[c].data();
            for (int k = 0; k < bins; ++k)
                pw[k] = sp[2 * k] * sp[2 * k] + sp[2 * k + 1] * sp[2 * k + 1];

            if (useSc)
            {
                float* ss = scSpec[c].data();
                for (int j = 0; j < N; ++j)
                    ss[j] = scRing[c][(size_t) ((ringPos + j) & mask)] * window[(size_t) j];
                std::fill (ss + N, ss + 2 * N, 0.0f);
                fft->performRealOnlyForwardTransform (ss, true);
                float* sw = scPower[c].data();
                for (int k = 0; k < bins; ++k)
                    sw[k] = ss[2 * k] * ss[2 * k] + ss[2 * k + 1] * ss[2 * k + 1];
            }
        }

        // --- détection (par canal et/ou liée)
        const float link = nc == 2 ? juce::jlimit (0.0f, 1.0f, settings.link) : 0.0f;
        auto det = [this] (int c) -> const float* { return useSc ? scPower[c].data() : power[c].data(); };

        if (link < 0.999f)
            for (int c = 0; c < nc; ++c)
                computeTargets (det (c), c, pathT[c].data());
        if (link > 0.001f)
        {
            const float* a = det (0);
            const float* b = det (1);
            for (int k = 0; k < bins; ++k) linkPower[(size_t) k] = 0.5f * (a[k] + b[k]);
            computeTargets (linkPower.data(), 2, pathT[2].data());
        }

        const bool listening = settings.listen;
        float peak = 0.0f;
        double eIn = 0.0, eOut = 0.0;     // énergies pondérées « K » (mesure + GAIN MATCH)
        for (int c = 0; c < nc; ++c)
        {
            float* t = target[c].data();
            const float* own = pathT[c].data();
            const float* lk = pathT[2].data();
            const float* wgt = weight[c].data();
            // FOCUS stéréo : répartit la quantité de traitement entre L/M et R/S
            const float fz = juce::jlimit (-1.0f, 1.0f, settings.focus);
            const float fm = nc < 2 ? 1.0f : (c == 0 ? std::min (1.0f, 1.0f - fz) : std::min (1.0f, 1.0f + fz));
            for (int k = 0; k < bins; ++k)
            {
                const float v = link >= 0.999f ? lk[k] : (link <= 0.001f ? own[k] : own[k] + link * (lk[k] - own[k]));
                tmpA[(size_t) k] = std::min (settings.maxCutDb, v * wgt[k] * fm);
            }
            // coupes lissées en fréquence (2 passes = forme en cloche)
            boxAverage (tmpA.data(), tmpB.data(), maskLo, maskHi);
            boxAverage (tmpB.data(), t, maskLo, maskHi);

            // attaque / relâchement par case
            float* r = red[c].data();
            float* sp = spec[c].data();
            const float* pw = power[c].data();
            for (int k = 0; k < bins; ++k)
            {
                const float tg = t[k];
                const float cf = tg > r[k] ? coefAtt[(size_t) k] : coefRel[(size_t) k];
                r[k] = tg + cf * (r[k] - tg);
                if (r[k] < 1.0e-4f) r[k] = 0.0f;
                peak = std::max (peak, r[k]);
                const float g = std::exp (-r[k] * 0.11512925f);   // 10^(-r/20)
                const float applied = listening ? (1.0f - g) * listenMask[(size_t) k] : g;
                eIn  += (double) (pw[k] * weightW[(size_t) k]);
                eOut += (double) (pw[k] * weightW[(size_t) k]) * (double) (g * g);
                if (zeroLatency)
                {
                    tmpB[(size_t) k] = listening ? std::log (std::max (1.0e-5f, applied)) : -r[k] * 0.11512925f;
                    continue;
                }
                sp[2 * k] *= applied;
                sp[2 * k + 1] *= applied;
            }

            if (zeroLatency)
            {
                // nouveau filtre ; l'ancien sert au fondu pendant la trame suivante
                std::swap (firPrev[c], firCur[c]);
                prevIdentity[c] = curIdentity[c];
                bool identity = ! listening;
                for (int k = 0; k < bins && identity; ++k) identity = r[k] <= 0.0f;
                curIdentity[c] = identity;
                if (! identity) designMinPhase (tmpB.data(), firCur[c].data());
                continue;
            }

            // synthèse + recouvrement-addition
            fft->performRealOnlyInverseTransform (sp);
            float* acc = outAcc[c].data();
            for (int j = 0; j < N; ++j)
                acc[j] += sp[j] * window[(size_t) j] * olaGain;

            float* q = outQueue[c].data();
            std::copy (acc, acc + hop, q);
            std::memmove (acc, acc + hop, sizeof (float) * (size_t) (N - hop));
            std::fill (acc + N - hop, acc + N, 0.0f);
        }

        firstFrame = false;
        peakReductionDb.store (peak);
        const float frameRed = eIn > 1.0e-12 ? (float) (10.0 * std::log10 (eIn / std::max (1.0e-30, eOut))) : 0.0f;
        overallReductionDb.store (frameRed);

        // GAIN MATCH : énergies moyennées lentement (~1,5 s), puis écart en dB
        {
            const double a = std::exp (-(double) frameSeconds / 1.5);
            loudIn  = eIn  + a * (loudIn  - eIn);
            loudOut = eOut + a * (loudOut - eOut);
            if (loudIn > 1.0e-6)
                makeupDb = juce::jlimit (0.0f, 12.0f, (float) (10.0 * std::log10 (loudIn / std::max (1.0e-30, loudOut))));
        }
        makeupDbOut.store (makeupDb);
        // écran : ~100 images/s suffisent, même en qualité Ultra
        publishTimer += frameSeconds;
        if (publishTimer >= 0.0099f) { publishTimer = 0.0f; publishUi (nc); }
    }

    void publishUi (int nc)
    {
        const juce::SpinLock::ScopedTryLockType sl (ui.lock);
        if (! sl.isLocked()) return;
        ui.bins = bins;
        ui.binHz = binHz;
        for (int k = 0; k < bins; ++k)
        {
            float pIn = 0.0f, rMax = 0.0f, pOut = 0.0f;
            for (int c = 0; c < nc; ++c)
            {
                const float r = red[c][(size_t) k];
                pIn += power[c][(size_t) k];
                pOut += power[c][(size_t) k] * std::exp (-r * 0.23025851f);
                rMax = std::max (rMax, r);
            }
            const float norm = 1.0f / (float) nc;
            ui.inDb[(size_t) k]  = 10.0f * std::log10 (pIn * norm + 1.0e-24f) - fsNorm;
            ui.outDb[(size_t) k] = 10.0f * std::log10 (pOut * norm + 1.0e-24f) - fsNorm;
            ui.redDb[(size_t) k] = rMax;
        }

        // Résonances suivies : maxima locaux de la réduction, les plus profonds d'abord,
        // espacés d'au moins 1/6 d'octave
        int n = 0;
        std::array<float, maxTracked> hz {}, db {};
        const float* r = ui.redDb.data();
        const int kMin = std::max (2, (int) (25.0f / binHz)), kMax = std::min (bins - 2, (int) (20000.0f / binHz));
        for (int k = kMin; k <= kMax; ++k)
        {
            const float v = r[k];
            if (v < 1.0f || v < r[k - 1] || v < r[k + 1]) continue;
            // position affinée (parabole)
            const float a = r[k - 1], b = v, c = r[k + 1];
            const float den = a - 2.0f * b + c;
            const float off = std::abs (den) > 1.0e-6f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / den) : 0.0f;
            const float f = ((float) k + off) * binHz;

            int near = -1;
            for (int i = 0; i < n; ++i)
                if (std::abs (std::log2 (f / hz[(size_t) i])) < 1.0f / 6.0f) { near = i; break; }
            if (near >= 0)
            {
                if (v > db[(size_t) near]) { hz[(size_t) near] = f; db[(size_t) near] = v; }
                continue;
            }
            if (n < maxTracked) { hz[(size_t) n] = f; db[(size_t) n] = v; ++n; continue; }
            int weakest = 0;
            for (int i = 1; i < n; ++i) if (db[(size_t) i] < db[(size_t) weakest]) weakest = i;
            if (v > db[(size_t) weakest]) { hz[(size_t) weakest] = f; db[(size_t) weakest] = v; }
        }
        ui.numTracked = n;
        ui.trackedHz = hz;
        ui.trackedDb = db;
        ++ui.counter;
    }

    // ----------------------------------------------------------------- état
    double sr = 44100.0;
    int scale = 1, quality = 1, timeQuality = 0, N = 2048, hop = 256, bins = 1025, nMax = 4096;
    float binHz = 21.5f, frameSeconds = 0.0058f, olaGain = 1.0f, fsNorm = 0.0f, levelNorm = 0.0f, slowCoef = 0.97f;
    std::array<std::unique_ptr<juce::dsp::FFT>, numQualities> ffts;
    juce::dsp::FFT* fft = nullptr;

    std::vector<float> inRing[maxChannels], scRing[maxChannels], dryRing[maxChannels];
    std::vector<float> outAcc[maxChannels], outQueue[maxChannels];
    std::vector<float> spec[maxChannels], scSpec[maxChannels];
    std::vector<float> power[maxChannels], scPower[maxChannels];
    std::vector<float> target[maxChannels], red[maxChannels], weight[maxChannels];
    std::vector<float> fineDb[3], slowDb[3], detPow[3], pathT[3];
    std::vector<float> tmpA, tmpB, tmpP;
    std::vector<double> prefix;
    std::vector<int> fineLo, fineHi, envLo, envHi, maskLo, maskHi;
    std::vector<float> thrDb, coefAtt, coefRel, coefDetUp, coefDetDown, pinkDb, listenMask;
    std::vector<float> window, linkPower, weightW, cep;
    std::vector<float> firCur[maxChannels], firPrev[maxChannels], hist[maxChannels], wetCur[maxChannels], wetPrev[maxChannels];
    bool curIdentity[maxChannels] { true, true }, prevIdentity[maxChannels] { true, true };
    bool zeroLatency = false;
    int firLen = 1024;
    float makeupDb = 0.0f;
    double loudIn = 0.0, loudOut = 0.0;

    int ringPos = 0, hopCount = 0, dryPos = 0, procCh = 2;
    float publishTimer = 1.0f;
    bool useSc = false, msActive = false, firstFrame = true;
    bool tablesValid = false;
    float tDetail = -1, tDetailTilt = 0, tAttack = 0, tRelease = 0, tAttackTilt = 0, tReleaseTilt = 0;
    bool tHard = false;
    Settings settings;

    juce::SmoothedValue<float> wetTrimSm { 1.0f }, outputSm { 1.0f }, mixSm { 1.0f }, bypassSm { 0.0f },
                               deltaSm { 0.0f }, listenSm { 0.0f }, makeupSm { 1.0f };
};
} // namespace velours

#pragma once
#include <cmath>
#include <algorithm>

// ============================================================================
//  Aociz SubShaper — moteur de traitement de la bande basse
//  Tourne à la fréquence suréchantillonnée (x4) pour limiter l'aliasing.
// ============================================================================

namespace subshaper
{
enum class Mode { Saturate = 0, Resonate, Octave, Synthesize, Overfold };

constexpr double kPi = 3.14159265358979323846;

// Filtre passe-bas 1 pôle
struct OnePoleLP
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float hz, double fs) { a = (float) std::exp (-2.0 * kPi * hz / fs); }
    float process (float x) { z = x + a * (z - x); return z; }
    void reset() { z = 0.0f; }
};

// Coupe-continu (DC blocker)
struct DCBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
    void prepare (double fs) { r = (float) (1.0 - 2.0 * kPi * 8.0 / fs); }
    float process (float x) { float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

// Suiveur d'enveloppe crête (attaque / relâchement)
struct EnvFollower
{
    float att = 0.0f, rel = 0.0f, env = 0.0f;
    void prepare (double fs, float attMs, float relMs)
    {
        att = (float) std::exp (-1.0 / (fs * attMs * 0.001));
        rel = (float) std::exp (-1.0 / (fs * relMs * 0.001));
    }
    float process (float x)
    {
        const float a = std::abs (x);
        const float c = a > env ? att : rel;
        env = a + c * (env - a);
        return env;
    }
    void reset() { env = 0.0f; }
};

// Suiveur RMS (pour la compensation automatique de niveau)
struct RmsFollower
{
    float c = 0.0f, ms = 0.0f;
    void prepare (double fs, float ms_) { c = (float) std::exp (-1.0 / (fs * ms_ * 0.001)); }
    float process (float x) { ms = x * x + c * (ms - x * x); return std::sqrt (ms); }
    void reset() { ms = 0.0f; }
};

// Filtre à variables d'état TPT (sortie passe-bande normalisée, gain 1 au centre)
struct SvfBandpass
{
    float s1 = 0.0f, s2 = 0.0f;
    float process (float x, float hz, float q, double fs)
    {
        const float g = (float) std::tan (kPi * std::min ((double) hz, fs * 0.45) / fs);
        const float R = 1.0f / (2.0f * q);
        const float h = 1.0f / (1.0f + 2.0f * R * g + g * g);
        const float hp = (x - (2.0f * R + g) * s1 - s2) * h;
        const float bp = g * hp + s1;
        s1 = g * hp + bp;
        const float lp = g * bp + s2;
        s2 = g * bp + lp;
        return bp * 2.0f * R; // normalisé
    }
    void reset() { s1 = s2 = 0.0f; }
};

// ----------------------------------------------------------------------------
//  État par canal
// ----------------------------------------------------------------------------
struct ChannelEngine
{
    double fs = 176400.0;

    // Suivi de hauteur (Octave / Synthèse)
    OnePoleLP trackA, trackB;
    EnvFollower env;
    bool  above = false;
    bool  flip  = false;
    float hyst  = 1.0e-4f;
    int   samplesSinceCross = 0;
    int   lastPeriod = 0;
    float freq  = 50.0f;
    double phase = 0.0;
    OnePoleLP subLpA, subLpB;
    OnePoleLP synthGate;

    // Résonance
    SvfBandpass bp;

    // Saturation / Repli
    DCBlocker dc;
    RmsFollower rmsIn, rmsOut;
    OnePoleLP gainSmooth;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        trackA.setCutoff (140.0f, fs);
        trackB.setCutoff (140.0f, fs);
        env.prepare (fs, 4.0f, 90.0f);
        dc.prepare (fs);
        rmsIn.prepare (fs, 60.0f);
        rmsOut.prepare (fs, 60.0f);
        gainSmooth.setCutoff (6.0f, fs);
        synthGate.setCutoff (30.0f, fs);
        reset();
    }

    void reset()
    {
        trackA.reset(); trackB.reset(); env.reset();
        above = flip = false; samplesSinceCross = 0; lastPeriod = 0; freq = 50.0f; phase = 0.0;
        subLpA.reset(); subLpB.reset(); synthGate.reset();
        bp.reset(); dc.reset(); rmsIn.reset(); rmsOut.reset();
        gainSmooth.z = 1.0f;
    }

    // Détection de front montant (passage par zéro avec hystérésis)
    bool risingEdge (float x)
    {
        const float t = trackB.process (trackA.process (x));
        if (samplesSinceCross < 1000000) ++samplesSinceCross;
        if (! above && t > hyst)
        {
            above = true;
            lastPeriod = samplesSinceCross;
            samplesSinceCross = 0;
            return true;
        }
        if (above && t < -hyst)   { above = false; }
        return false;
    }

    // Compensation automatique de niveau pour les modes non linéaires
    float autoGain (float in, float out)
    {
        const float ri = rmsIn.process (in);
        const float ro = rmsOut.process (out);
        float g = ro > 1.0e-6f ? ri / ro : 1.0f;
        g = std::clamp (g, 0.05f, 4.0f);
        return out * gainSmooth.process (g);
    }

    // amount et character : 0..1
    float process (float x, Mode mode, float amount, float character)
    {
        switch (mode)
        {
            case Mode::Saturate:
            {
                // Drive 1..25, asymétrie = harmoniques paires
                const float drive = 1.0f + amount * amount * 24.0f;
                const float bias  = character * 0.5f;
                float y = std::tanh (drive * x + bias) - std::tanh (bias);
                y = dc.process (y);
                return autoGain (x, y);
            }

            case Mode::Resonate:
            {
                // Pic résonant entre 30 et 200 Hz, Q de 0,7 à 12
                const float hz = 30.0f * std::pow (200.0f / 30.0f, character);
                const float q  = 0.7f + amount * 11.3f;
                const float r  = bp.process (x, hz, q, fs);
                return x + r * amount * 2.5f;
            }

            case Mode::Octave:
            {
                // Diviseur de fréquence (flip-flop) -> carré à f/2 -> filtré en quasi-sinus
                if (risingEdge (x)) flip = ! flip;
                const float e   = env.process (x);
                const float sq  = flip ? 1.0f : -1.0f;
                const float cut = 40.0f + character * 180.0f;
                subLpA.setCutoff (cut, fs);
                subLpB.setCutoff (cut, fs);
                const float sub = subLpB.process (subLpA.process (sq * e));
                return x + sub * amount * 1.6f;
            }

            case Mode::Synthesize:
            {
                // Suivi de hauteur + oscillateur sinus ; Caractère = part de remplacement
                if (risingEdge (x))
                {
                    const float measured = (float) (fs / std::max (1, lastPeriod));
                    if (measured > 25.0f && measured < 300.0f)
                        freq += 0.35f * (measured - freq);
                }
                const float e = env.process (x);
                const float gate = synthGate.process (e > 1.0e-3f ? 1.0f : 0.0f);
                phase += freq / fs;
                if (phase >= 1.0) phase -= 1.0;
                const float sine = (float) std::sin (2.0 * kPi * phase) * e * gate;
                return x * (1.0f - character) + sine * amount * 1.2f;
            }

            case Mode::Overfold:
            {
                // Wavefolder sinusoïdal
                const float pre  = x * (1.0f + amount * amount * 14.0f);
                const float bias = character * 0.6f;
                const float half = (float) (kPi * 0.5);
                float y = std::sin (half * (pre + bias)) - std::sin (half * bias);
                y = dc.process (y);
                return autoGain (x, y);
            }
        }
        return x;
    }
};
} // namespace subshaper

#pragma once
#include <cmath>
#include <algorithm>

// ============================================================================
//  Subshaper — moteur DSP (chaîne cumulable GEN -> TONE -> DRIVE)
//  Tourne à la fréquence suréchantillonnée.
// ============================================================================
namespace subshaper
{
constexpr double kPi = 3.14159265358979323846;

struct OnePoleLP
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float hz, double fs) { a = (float) std::exp (-2.0 * kPi * hz / fs); }
    float process (float x) { z = x + a * (z - x); return z; }
    void reset() { z = 0.0f; }
};

struct DCBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
    void prepare (double fs) { r = (float) (1.0 - 2.0 * kPi * 8.0 / fs); }
    float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

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
        env = a + (a > env ? att : rel) * (env - a);
        return env;
    }
    void reset() { env = 0.0f; }
};

struct RmsFollower
{
    float c = 0.0f, ms = 0.0f;
    void prepare (double fs, float timeMs) { c = (float) std::exp (-1.0 / (fs * timeMs * 0.001)); }
    float process (float x) { ms = x * x + c * (ms - x * x); return std::sqrt (ms); }
    void reset() { ms = 0.0f; }
};

// Passe-bande TPT normalisé (gain 1 à la fréquence centrale)
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
        return bp * 2.0f * R;
    }
    void reset() { s1 = s2 = 0.0f; }
};

// ----------------------------------------------------------------------------
struct EngineParams
{
    float keyFreq = 32.7f;

    float genEn = 0.0f, genLevel = 0.4f, genTone = 0.3f;
    int   genType = 0;          // 0 = Octave, 1 = Sine
    bool  keyLock = false;

    float toneEn = 0.0f, toneAmt = 0.3f, toneQ = 0.3f;
    int   toneHarm = 0;         // 0..3 -> x1..x4

    float driveEn = 0.0f, drive = 0.35f, color = 0.2f, focus = 0.0f, driveMix = 1.0f;
    int   driveType = 0;        // Tape, Tube, Hard, Fold
};

// ----------------------------------------------------------------------------
struct ChannelEngine
{
    double fs = 176400.0;

    // Suivi de hauteur
    OnePoleLP trackA, trackB;
    EnvFollower env;
    bool  above = false, flip = false;
    int   samplesSinceCross = 0, lastPeriod = 0;
    float trackedFreq = 50.0f;
    double phase = 0.0;
    OnePoleLP subLpA, subLpB, gate;

    // Tone
    SvfBandpass toneBp;

    // Drive
    SvfBandpass focusPre, focusPost;
    DCBlocker dc;
    RmsFollower rmsIn, rmsOut;
    OnePoleLP gainSmooth;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        trackA.setCutoff (140.0f, fs);
        trackB.setCutoff (140.0f, fs);
        env.prepare (fs, 4.0f, 90.0f);
        gate.setCutoff (30.0f, fs);
        dc.prepare (fs);
        rmsIn.prepare (fs, 60.0f);
        rmsOut.prepare (fs, 60.0f);
        gainSmooth.setCutoff (6.0f, fs);
        reset();
    }

    void reset()
    {
        trackA.reset(); trackB.reset(); env.reset(); gate.reset();
        above = flip = false; samplesSinceCross = lastPeriod = 0;
        trackedFreq = 50.0f; phase = 0.0;
        subLpA.reset(); subLpB.reset();
        toneBp.reset(); focusPre.reset(); focusPost.reset();
        dc.reset(); rmsIn.reset(); rmsOut.reset();
        gainSmooth.z = 1.0f;
    }

    bool risingEdge (float x)
    {
        const float t = trackB.process (trackA.process (x));
        if (samplesSinceCross < 1000000) ++samplesSinceCross;
        if (! above && t > 1.0e-4f)
        {
            above = true;
            lastPeriod = samplesSinceCross;
            samplesSinceCross = 0;
            return true;
        }
        if (above && t < -1.0e-4f) above = false;
        return false;
    }

    // --- GEN ---------------------------------------------------------------
    float processGen (float x, const EngineParams& p)
    {
        const bool edge = risingEdge (x);
        const float e = env.process (x);

        if (p.genType == 0) // Octave
        {
            if (edge) flip = ! flip;
            const float cut = 40.0f + p.genTone * 180.0f;
            subLpA.setCutoff (cut, fs);
            subLpB.setCutoff (cut, fs);
            const float sub = subLpB.process (subLpA.process ((flip ? 1.0f : -1.0f) * e));
            return x + sub * p.genLevel * 1.6f;
        }

        // Sine
        if (edge)
        {
            const float measured = (float) (fs / std::max (1, lastPeriod));
            if (measured > 25.0f && measured < 300.0f)
                trackedFreq += 0.35f * (measured - trackedFreq);
        }
        const float f = p.keyLock ? p.keyFreq : trackedFreq;
        const float g = gate.process (e > 1.0e-3f ? 1.0f : 0.0f);
        phase += f / fs;
        if (phase >= 1.0) phase -= 1.0;
        const float sine = (float) std::sin (2.0 * kPi * phase) * e * g;
        return x * (1.0f - p.genTone) + sine * p.genLevel * 2.0f;
    }

    // --- TONE --------------------------------------------------------------
    float processTone (float x, const EngineParams& p)
    {
        const float hz = p.keyFreq * (float) (p.toneHarm + 1);
        const float q  = 1.0f + p.toneQ * 15.0f;
        return x + toneBp.process (x, hz, q, fs) * p.toneAmt * 2.0f;
    }

    // --- DRIVE -------------------------------------------------------------
    static float softClip (float u) { return u / std::pow (1.0f + std::pow (std::abs (u), 8.0f), 0.125f); }
    static float tube (float u)     { return u >= 0.0f ? std::tanh (u) : std::tanh (0.6f * u) / 0.6f * 0.7f; }

    float shape (float u, int type, float bias) const
    {
        const float half = (float) (kPi * 0.5);
        switch (type)
        {
            case 0:  return std::tanh (u + bias) - std::tanh (bias);
            case 1:  return tube (u + bias) - tube (bias);
            case 2:  return softClip (u + bias) - softClip (bias);
            default: return std::sin (half * (u + bias)) - std::sin (half * bias);
        }
    }

    float processDrive (float x, const EngineParams& p)
    {
        // Accentuation de la fondamentale (note KEY) avant saturation
        const float k = p.focus * 2.0f;
        float xf = x;
        if (k > 0.0f)
            xf = x + k * focusPre.process (x, p.keyFreq, 2.0f, fs);

        const float gain = p.driveType == 3 ? 1.0f + p.drive * p.drive * 14.0f
                                            : 1.0f + p.drive * p.drive * 24.0f;
        const float bias = p.color * (p.driveType == 3 ? 0.6f : 0.4f);
        float y = shape (xf * gain, p.driveType, bias);

        if (k > 0.0f)
            y -= (k / (1.0f + k)) * focusPost.process (y, p.keyFreq, 2.0f, fs);

        y = dc.process (y);

        // Compensation automatique de niveau
        const float ri = rmsIn.process (x);
        const float ro = rmsOut.process (y);
        const float g = std::clamp (ro > 1.0e-6f ? ri / ro : 1.0f, 0.05f, 4.0f);
        y *= gainSmooth.process (g);

        return x + p.driveMix * (y - x);
    }

    float process (float x, const EngineParams& p)
    {
        float y = x;
        if (p.genEn > 0.0f)   y += p.genEn   * (processGen (y, p)   - y);
        if (p.toneEn > 0.0f)  y += p.toneEn  * (processTone (y, p)  - y);
        if (p.driveEn > 0.0f) y += p.driveEn * (processDrive (y, p) - y);
        return y;
    }
};

// ----------------------------------------------------------------------------
//  SHAPE : transient designer sur le grave (fréquence de base)
// ----------------------------------------------------------------------------
struct TransientShaper
{
    EnvFollower fast, slow, susShort, susLong;

    void prepare (double fs)
    {
        fast.prepare (fs, 0.3f, 40.0f);
        slow.prepare (fs, 20.0f, 40.0f);
        susShort.prepare (fs, 1.0f, 40.0f);
        susLong.prepare (fs, 1.0f, 400.0f);
        reset();
    }
    void reset() { fast.reset(); slow.reset(); susShort.reset(); susLong.reset(); }

    // attack / sustain : -1..+1
    float process (float x, float attack, float sustain)
    {
        const float ef = fast.process (x), es = slow.process (x);
        const float t = ef > 1.0e-6f ? std::max (0.0f, (ef - es) / ef) : 0.0f;
        const float sl = susLong.process (x), ss = susShort.process (x);
        const float s = sl > 1.0e-6f ? std::max (0.0f, (sl - ss) / sl) : 0.0f;
        const float dB = std::clamp (attack * 15.0f * t + sustain * 18.0f * s, -30.0f, 15.0f);
        return x * std::pow (10.0f, dB / 20.0f);
    }
};
} // namespace subshaper

#pragma once

#include <JuceHeader.h>
#include <cmath>

//==============================================================================
//  Allpass d'ordre 2 pour la paire de Hilbert.
//==============================================================================
struct Allpass2
{
    float a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    inline void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = a2 * (x + y2) - x2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

//==============================================================================
//  Frequency shifter SSB.
//  Ici il ne sert QUE dans la boucle de feedback du delay : chaque repetition
//  remonte d'un cran, ce qui produit un escalier ascendant qui ne redescend
//  jamais. Sur le signal direct il rendait tout metallique.
//==============================================================================
class FreqShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        static const float coefA[4] = { 0.6923877778065f, 0.9360654322959f,
                                        0.9882295226860f, 0.9987488452737f };
        static const float coefB[4] = { 0.4021921162426f, 0.8561710882420f,
                                        0.9722909545651f, 0.9952884791278f };

        for (int i = 0; i < 4; ++i)
        {
            chainA[i].a2 = coefA[i];
            chainB[i].a2 = coefB[i];
        }
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 4; ++i) { chainA[i].reset(); chainB[i].reset(); }
        delayed = 0.0f;
        phase   = 0.0;
    }

    inline float process (float x, float shiftHz, float wet) noexcept
    {
        float a = x;
        for (int i = 0; i < 4; ++i) a = chainA[i].process (a);

        float b = x;
        for (int i = 0; i < 4; ++i) b = chainB[i].process (b);

        const float bd = delayed;
        delayed = b;

        phase += shiftHz / sr;
        while (phase >= 1.0) phase -= 1.0;
        while (phase <  0.0) phase += 1.0;

        const float ang = (float) (juce::MathConstants<double>::twoPi * phase);
        const float shifted = bd * std::cos (ang) - a * std::sin (ang);

        return x * (1.0f - wet) + shifted * wet;
    }

private:
    Allpass2 chainA[4], chainB[4];
    float  delayed = 0.0f;
    double phase   = 0.0;
    double sr      = 44100.0;
};

//==============================================================================
//  RISER
//
//  La hauteur est pilotee DIRECTEMENT par la position du potard : a 0 % la
//  fondamentale est en bas, a 100 % elle est en haut, 4,5 octaves plus loin.
//  C'est le point qui manquait dans les versions precedentes, ou un LFO libre
//  faisait monter puis redescendre la hauteur independamment du potard : rien
//  ne progressait, ca rebouclait.
//
//  Les partiels sont harmoniques (1, 2, 3, 4, 5) et non espaces d'une octave :
//  on entend donc une seule hauteur, franche, qui monte une fois et arrive.
//==============================================================================
class Riser
{
public:
    static constexpr int H = 5;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset()
    {
        for (int i = 0; i < H; ++i) { phase[i] = 0.0; inc[i] = 0.0; amp[i] = 0.0f; }
    }

    void update (float intensity)
    {
        const double f0  = baseHz * std::pow (2.0, (double) intensity * octaveSpan);
        const double nyq = sr * 0.45;

        float total = 0.0f;
        for (int i = 0; i < H; ++i)
        {
            const double f = f0 * (double) (i + 1);
            if (f >= nyq) { amp[i] = 0.0f; inc[i] = 0.0; continue; }
            amp[i] = 1.0f / (float) (i + 1);
            inc[i] = f / sr;
            total += amp[i];
        }
        if (total > 0.0f)
            for (int i = 0; i < H; ++i) amp[i] /= total;

        currentF0 = f0;
    }

    inline float process() noexcept
    {
        float out = 0.0f;
        for (int i = 0; i < H; ++i)
        {
            if (amp[i] <= 0.0f) continue;
            out += amp[i] * (float) std::sin (juce::MathConstants<double>::twoPi * phase[i]);
            phase[i] += inc[i];
            if (phase[i] >= 1.0) phase[i] -= 1.0;
        }
        return out;
    }

    double getFundamental() const noexcept { return currentF0; }

private:
    double sr         = 44100.0;
    double baseHz     = 110.0;
    double octaveSpan = 4.5;
    double currentF0  = 110.0;
    double phase[H] {};
    double inc[H]   {};
    float  amp[H]   {};
};

//==============================================================================
//  BARBER POLE FILTER
//
//  Meme illusion, mais appliquee au signal d'entree plutot qu'a des sinus.
//  Six passe-bande dont les frequences centrales glissent vers le haut en
//  permanence, espacees regulierement sur sept octaves, chacun avec sa propre
//  fenetre d'amplitude. C'est ce qui fait "monter" un morceau complet : ce sont
//  ses propres frequences que l'on entend defiler vers le haut.
//==============================================================================
class BarberFilter
{
public:
    static constexpr int N = 6;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        for (int i = 0; i < N; ++i)
        {
            bp[i].prepare (spec);
            bp[i].setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            bp[i].setResonance (2.2f);
            bp[i].setCutoffFrequency (500.0f);
            bp[i].reset();
            w[i] = 0.0f;
        }
        sr = spec.sampleRate;
        phase = 0.0;
    }

    void reset()
    {
        for (int i = 0; i < N; ++i) bp[i].reset();
        phase = 0.0;
    }

    //  pilote par le potard : a 0 % les bandes sont en bas, a 100 % elles ont
    //  balaye 85 % de l'etendue. Jamais de bouclage pendant une montee.
    void update (float intensity)
    {
        phase = (double) intensity * 0.85 * spanOctaves;

        const double nyq = sr * 0.45;

        for (int i = 0; i < N; ++i)
        {
            double p = phase + spanOctaves * (double) i / (double) N;
            while (p >= spanOctaves) p -= spanOctaves;

            const double freq = fMin * std::pow (2.0, p);
            const double norm = p / spanOctaves;              // 0..1
            const float  win  = (float) std::pow (std::sin (juce::MathConstants<double>::pi * norm), 2.0);

            w[i] = win;
            bp[i].setCutoffFrequency ((float) juce::jlimit (20.0, nyq, freq));
        }
    }

    inline float process (int channel, float x) noexcept
    {
        float sum = 0.0f;
        for (int i = 0; i < N; ++i)
            sum += bp[i].processSample (channel, x) * w[i];
        return sum * 2.8f;   // compense la perte du banc de passe-bande (mesuree : -13 dB sans cela)
    }

    void snapToZero() noexcept
    {
        for (int i = 0; i < N; ++i) bp[i].snapToZero();
    }

private:
    juce::dsp::StateVariableTPTFilter<float> bp[N];
    float  w[N] {};
    double sr           = 44100.0;
    double phase        = 0.0;
    double fMin         = 70.0;
    double spanOctaves  = 7.0;
};

//==============================================================================
inline float softClip (float x) noexcept { return std::tanh (x); }

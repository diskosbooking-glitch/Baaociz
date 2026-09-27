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
//  BARBER POLE FILTER
//
//  Illusion de montee appliquee au signal d'entree lui-meme : ce filtre
//  n'ajoute aucun son, il fait defiler vers le haut les frequences du morceau.
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
//  NOISE RISER  (v0.5 : un souffle qui monte, sans aucune note)
//
//  v0.3 ajoutait une sinusoide qui montait jusqu'a 2,5 kHz. v0.4 la remplacait
//  par deux bandes de bruit tres resonantes (Q jusqu'a 28) : sur les presets
//  durs, elles sifflaient comme une note et on retrouvait le meme defaut.
//
//  Ici plus rien de tonal :
//   - le souffle passe dans un passe-haut puis un passe-bas Butterworth
//     (Q = 0,707 : aucune bosse de resonance, gain crete mesure -0,4 dB),
//     places une octave de part et d'autre du centre. Deux octaves de large :
//     on entend du bruit qui s'eclaircit, jamais une hauteur.
//   - le centre suit la course validee en v0.3 : 220 Hz a 0 %, 5 kHz a 100 %.
//   - le lit de bruit large de la v0.4 (le cote noye) est garde tel quel.
//==============================================================================
class NoiseRiser
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;

        sweepHp.prepare (spec);
        sweepHp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        sweepHp.setResonance (butterworthQ);

        sweepLp.prepare (spec);
        sweepLp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        sweepLp.setResonance (butterworthQ);

        bed.prepare (spec);
        bed.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        bed.setResonance (0.7f);

        update (0.0f);
        reset();
    }

    void reset() { sweepHp.reset(); sweepLp.reset(); bed.reset(); }

    void update (float intensity)
    {
        const double nyq    = sr * 0.45;
        const double centre = startHz * std::pow (2.0, (double) intensity * octaveSpan);

        sweepHp.setCutoffFrequency ((float) juce::jlimit (20.0, nyq, centre * 0.5));
        sweepLp.setCutoffFrequency ((float) juce::jlimit (40.0, nyq, centre * 2.0));
        bed.setCutoffFrequency     ((float) juce::jlimit (100.0, nyq,
                                     400.0 + 3000.0 * (double) intensity));
    }

    inline float process (int channel, float noise, float sweepGain, float bedGain) noexcept
    {
        const float sw = sweepLp.processSample (channel, sweepHp.processSample (channel, noise));
        const float bd = bed.processSample (channel, noise);
        return sw * sweepGain + bd * bedGain;
    }

    void snapToZero() noexcept { sweepHp.snapToZero(); sweepLp.snapToZero(); bed.snapToZero(); }

private:
    static constexpr float  butterworthQ = 0.70710678f;
    static constexpr double startHz      = 220.0;
    static constexpr double octaveSpan   = 4.5;

    juce::dsp::StateVariableTPTFilter<float> sweepHp, sweepLp, bed;
    double sr = 44100.0;
};

//==============================================================================
inline float softClip (float x) noexcept { return std::tanh (x); }

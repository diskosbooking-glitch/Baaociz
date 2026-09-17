#pragma once

#include <JuceHeader.h>
#include <cmath>

//==============================================================================
//  Second order allpass section used to build a Hilbert transform pair.
//  y[n] = a2 * (x[n] + y[n-2]) - x[n-2]
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
//  Single sideband frequency shifter (the "barber pole" flavour).
//
//  Unlike a pitch shifter, every partial is moved by the SAME number of hertz,
//  which destroys the harmonic ratios and produces that never ending climbing
//  sensation. Two allpass chains give us a signal pair that is ~90 degrees
//  apart over the audio band (Olli Niemitalo's classic coefficients), which we
//  then rotate with a quadrature oscillator.
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

    void setPhaseOffset (double p) noexcept { phase = p; }

    inline float process (float x, float shiftHz, float wet) noexcept
    {
        float a = x;
        for (int i = 0; i < 4; ++i) a = chainA[i].process (a);

        float b = x;
        for (int i = 0; i < 4; ++i) b = chainB[i].process (b);

        const float bd = delayed;   // one sample delay on the B path
        delayed = b;

        phase += shiftHz / sr;
        while (phase >= 1.0) phase -= 1.0;
        while (phase < 0.0)  phase += 1.0;

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
//  Bit + sample rate reduction, per channel state.
//==============================================================================
struct Crusher
{
    int   counter = 0;
    float hold    = 0.0f;

    inline void reset() noexcept { counter = 0; hold = 0.0f; }

    inline float process (float x, int step, float levels) noexcept
    {
        if (--counter <= 0)
        {
            counter = step;
            hold = (levels > 1.0f) ? std::round (x * levels) / levels : x;
        }
        return hold;
    }
};

//==============================================================================
//  Soft saturation + gentle output ceiling.
//==============================================================================
inline float softClip (float x) noexcept
{
    return std::tanh (x);
}

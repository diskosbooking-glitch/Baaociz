#pragma once

#include <JuceHeader.h>
#include <cmath>

//==============================================================================
//  Chaque preset decrit, pour chaque module de la chaine, DEUX choses :
//    - amount : la valeur atteinte quand le potard Intensity est a 100 %
//    - exp    : la courbe (1 = lineaire, >1 = arrive tard, <1 = arrive tot)
//
//  C'est la vraie "recette" du plugin : le DSP est classique, tout le
//  caractere vient de la maniere dont chaque module monte le long du potard.
//==============================================================================

enum ModIndex
{
    ModHighpass = 0,   // le grave qui disparait
    ModLowpass,        // l'aigu qui se referme
    ModDrive,          // saturation
    ModCrush,          // bitcrush / decimation
    ModPhaser,         // phaser
    ModShift,          // frequency shifter (barber pole)
    ModDelay,          // niveau du delay
    ModFeedback,       // feedback du delay
    ModReverb,         // niveau de reverb
    ModNoise,          // generateur de bruit filtre (riser)
    NumMods
};

struct ModCurve
{
    float amount;
    float exp;
};

struct PresetDef
{
    const char* name;
    ModCurve m[NumMods];
};

//  HP        LP        DRIVE     CRUSH     PHASER    SHIFT     DELAY     FBACK     REVERB    NOISE
static const PresetDef kPresets[] =
{
    { "Warm Up",
      { {0.55f,1.8f}, {0.25f,2.2f}, {0.30f,2.0f}, {0.00f,1.0f}, {0.25f,1.6f},
        {0.10f,2.4f}, {0.45f,1.4f}, {0.40f,1.6f}, {0.45f,1.5f}, {0.20f,2.6f} } },

    { "Fist Pump",
      { {0.75f,1.5f}, {0.35f,2.0f}, {0.45f,1.8f}, {0.00f,1.0f}, {0.35f,1.5f},
        {0.18f,2.2f}, {0.70f,1.2f}, {0.62f,1.3f}, {0.55f,1.4f}, {0.45f,2.2f} } },

    { "Balloon Head",
      { {0.60f,1.6f}, {0.20f,2.4f}, {0.35f,2.0f}, {0.00f,1.0f}, {0.70f,1.1f},
        {0.55f,1.5f}, {0.50f,1.4f}, {0.55f,1.4f}, {0.60f,1.3f}, {0.25f,2.6f} } },

    { "Tunnel Vision",
      { {0.85f,1.2f}, {0.70f,1.3f}, {0.40f,1.9f}, {0.00f,1.0f}, {0.30f,1.6f},
        {0.20f,2.2f}, {0.60f,1.3f}, {0.68f,1.2f}, {0.85f,1.1f}, {0.35f,2.4f} } },

    { "Barber Pole",
      { {0.65f,1.5f}, {0.30f,2.2f}, {0.35f,2.0f}, {0.00f,1.0f}, {0.40f,1.5f},
        {0.95f,1.0f}, {0.65f,1.2f}, {0.70f,1.2f}, {0.65f,1.3f}, {0.20f,2.8f} } },

    { "Rocket Fuel",
      { {0.80f,1.3f}, {0.30f,2.2f}, {0.70f,1.4f}, {0.12f,2.8f}, {0.30f,1.8f},
        {0.30f,2.0f}, {0.55f,1.4f}, {0.55f,1.4f}, {0.60f,1.3f}, {0.90f,1.6f} } },

    { "Jaw Drop",
      { {0.90f,1.1f}, {0.55f,1.6f}, {0.85f,1.2f}, {0.35f,2.4f}, {0.55f,1.3f},
        {0.60f,1.4f}, {0.75f,1.1f}, {0.75f,1.1f}, {0.70f,1.2f}, {0.70f,1.8f} } },

    { "Meltdown",
      { {0.95f,1.0f}, {0.75f,1.2f}, {1.00f,1.0f}, {0.75f,1.6f}, {0.80f,1.1f},
        {0.85f,1.1f}, {0.85f,1.0f}, {0.85f,1.0f}, {0.90f,1.0f}, {1.00f,1.3f} } }
};

static constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (PresetDef));

inline juce::StringArray getPresetNames()
{
    juce::StringArray names;
    for (int i = 0; i < kNumPresets; ++i)
        names.add (kPresets[i].name);
    return names;
}

//  Valeur d'un module pour une intensite t (0..1)
inline float modValue (const PresetDef& p, int index, float t) noexcept
{
    const ModCurve& c = p.m[index];
    if (c.amount <= 0.0f) return 0.0f;
    return c.amount * std::pow (juce::jlimit (0.0f, 1.0f, t), c.exp);
}

#pragma once

#include <JuceHeader.h>
#include <cmath>

//==============================================================================
//  amount = valeur atteinte a 100 % du potard
//  exp    = courbe (1 = lineaire, >1 = arrive tard, <1 = arrive tot)
//==============================================================================

enum ModIndex
{
    ModHighpass = 0,   // passe-haut resonant : le grave s'efface
    ModLowpass,        // passe-bas : l'aigu se referme
    ModBarber,         // filtre barber pole sur le signal source
    ModRiser,          // niveau du riser harmonique (hauteur pilotee par le potard)
    ModNoise,          // riser de bruit filtre
    ModDrive,          // saturation
    ModDelay,          // niveau du delay
    ModFeedback,       // feedback du delay
    ModShift,          // frequency shifter DANS le feedback
    ModReverb,         // reverbe
    ModGate,           // gate synchro tempo qui s'accelere
    NumMods
};

struct ModCurve { float amount; float exp; };

struct PresetDef
{
    const char* name;
    ModCurve m[NumMods];
};

//       HP           LP           BARBER       RISER        NOISE
//       DRIVE        DELAY        FBACK        SHIFT        REVERB       GATE
static const PresetDef kPresets[] =
{
    { "Warm Up",
      { {0.45f,1.8f}, {0.15f,2.4f}, {0.35f,1.6f}, {0.30f,2.0f}, {0.20f,2.6f},
        {0.15f,2.2f}, {0.40f,1.5f}, {0.45f,1.5f}, {0.25f,1.8f}, {0.45f,1.5f}, {0.00f,1.0f} } },

    { "Fist Pump",
      { {0.70f,1.4f}, {0.20f,2.4f}, {0.45f,1.4f}, {0.55f,1.6f}, {0.45f,2.2f},
        {0.30f,1.8f}, {0.60f,1.3f}, {0.60f,1.3f}, {0.35f,1.6f}, {0.50f,1.4f}, {0.35f,2.2f} } },

    { "Balloon Head",
      { {0.55f,1.6f}, {0.15f,2.5f}, {0.65f,1.3f}, {0.85f,1.3f}, {0.20f,2.8f},
        {0.20f,2.0f}, {0.45f,1.5f}, {0.55f,1.4f}, {0.55f,1.4f}, {0.60f,1.3f}, {0.00f,1.0f} } },

    { "Tunnel Vision",
      { {0.80f,1.2f}, {0.60f,1.4f}, {0.70f,1.2f}, {0.40f,1.9f}, {0.35f,2.4f},
        {0.25f,2.0f}, {0.55f,1.3f}, {0.65f,1.2f}, {0.30f,1.8f}, {0.85f,1.1f}, {0.00f,1.0f} } },

    { "Barber Pole",
      { {0.55f,1.6f}, {0.20f,2.4f}, {0.95f,1.0f}, {0.65f,1.4f}, {0.15f,2.8f},
        {0.20f,2.0f}, {0.70f,1.2f}, {0.75f,1.1f}, {0.90f,1.0f}, {0.60f,1.3f}, {0.00f,1.0f} } },

    { "Rocket Fuel",
      { {0.75f,1.3f}, {0.25f,2.3f}, {0.50f,1.5f}, {0.70f,1.5f}, {0.90f,1.6f},
        {0.50f,1.5f}, {0.50f,1.4f}, {0.55f,1.4f}, {0.35f,1.7f}, {0.55f,1.3f}, {0.45f,2.0f} } },

    { "Jaw Drop",
      { {0.85f,1.1f}, {0.45f,1.7f}, {0.75f,1.2f}, {0.75f,1.3f}, {0.65f,1.8f},
        {0.70f,1.3f}, {0.70f,1.2f}, {0.70f,1.2f}, {0.50f,1.5f}, {0.65f,1.2f}, {0.70f,1.6f} } },

    { "Meltdown",
      { {0.92f,1.0f}, {0.65f,1.3f}, {0.90f,1.0f}, {0.90f,1.1f}, {0.95f,1.3f},
        {0.90f,1.1f}, {0.80f,1.1f}, {0.80f,1.1f}, {0.70f,1.2f}, {0.80f,1.1f}, {0.85f,1.3f} } }
};

static constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (PresetDef));

inline juce::StringArray getPresetNames()
{
    juce::StringArray names;
    for (int i = 0; i < kNumPresets; ++i)
        names.add (kPresets[i].name);
    return names;
}

inline float modValue (const PresetDef& p, int index, float t) noexcept
{
    const ModCurve& c = p.m[index];
    if (c.amount <= 0.0f) return 0.0f;
    return c.amount * std::pow (juce::jlimit (0.0f, 1.0f, t), c.exp);
}

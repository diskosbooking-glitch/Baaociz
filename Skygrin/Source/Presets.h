#pragma once

#include <JuceHeader.h>
#include <array>
#include "Mapping.h"

//==============================================================================
//  SKYGRIN - PRESETS (VOICINGS)
//
//  Un preset = une courbe de reponse par sous-effet. Un module absent d'un
//  preset est coupe (Curve{} -> amount = 0).
//
//  Echelle de chaque colonne (valeur normalisee 0..1 -> unite reelle), voir
//  mapToSettings() dans PluginProcessor.cpp :
//
//    highpass  logMap 20 Hz -> 3,2 kHz   (0.55 = 327 Hz, 0.62 = 466 Hz, 0.70 = 697 Hz)
//    lowpass   20 kHz -> 500 Hz           (le haut se referme : effet tunnel)
//    air       shelf +0..+9 dB a 4,5 kHz  (la brillance au sommet)
//    pitch     0..+12 demi-tons           (0.025 = +30 cents, 1.0 = une octave)
//    barber    mix du barber pole filter  (les frequences du morceau defilent vers le haut)
//    noise     niveau du souffle          (bruit non resonant, 220 Hz -> 5 kHz, aucune note)
//    delay     mix 0..55 %                feedback 0..70 %
//    shift     0..55 Hz de frequency shift dans la boucle du delay
//    gate      profondeur du gate synchro (1/8 -> 1/16 -> 1/32)
//    reverb    mix + taille + decay
//    comp      seuil 0 -> -24 dB, ratio 1:1 -> 6:1, makeup automatique
//    drive     0..+22 dB dans le soft-clipper (tanh, sur-echantillonne x2)
//
//  delayFrom / delayTo : valeurs de note du delay synchro tempo
//  (4 = noire, 8 = croche, 16 = double croche, 32 = triple croche).
//  La division double par paliers quand le potard monte : 1/4 -> 1/8 -> 1/16.
//
//  pitchWet : 1.0 = tout le morceau monte (effet bande qui accelere),
//             0.3 = une copie qui monte par-dessus le signal d'origine.
//==============================================================================

using skygrin::Curve;
using skygrin::pw;
using skygrin::sc;
using skygrin::ex;

struct PresetDef
{
    const char* name = "";

    // ---- courbes, dans l'ordre de la chaine audio -------------------------
    Curve highpass, lowpass, air, pitch, barber, noise,
          delay, feedback, shift, gate, reverb, comp, drive;

    // ---- reglages musicaux -------------------------------------------------
    int   delayFrom = 4;
    int   delayTo   = 16;
    float pitchWet  = 0.4f;
};

// Index des modules : sert au lissage generique dans le processeur.
enum ModIndex
{
    ModHighpass = 0, ModLowpass, ModAir, ModPitch, ModBarber, ModNoise,
    ModDelay, ModFeedback, ModShift, ModGate, ModReverb, ModComp, ModDrive,
    NumMods
};

// Table ModIndex -> membre de PresetDef (pointeurs sur membres).
inline constexpr std::array<Curve PresetDef::*, NumMods> kCurveOf
{
    &PresetDef::highpass, &PresetDef::lowpass, &PresetDef::air,   &PresetDef::pitch,
    &PresetDef::barber,   &PresetDef::noise,   &PresetDef::delay, &PresetDef::feedback,
    &PresetDef::shift,    &PresetDef::gate,    &PresetDef::reverb, &PresetDef::comp,
    &PresetDef::drive
};

//==============================================================================
//  Un module non cite dans un preset est volontairement a zero : on coupe
//  l'avertissement "missing initializer" pour cette table uniquement.
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wmissing-field-initializers", "-Wmissing-designated-field-initializers")

//==============================================================================
//  Les 8 premiers presets reprennent exactement les courbes de la v0.5
//  (on ne perd rien de ce qui sonnait) ; on y ajoute air / pitch / comp.
//  Les 4 derniers exploitent les nouvelles formes de courbes.
//==============================================================================
inline const PresetDef kPresets[] =
{
    // Montee douce : le grave s'efface, un peu d'espace, rien d'agressif.
    { .name = "Warm Up",
      .highpass = pw (0.45f, 1.8f), .lowpass = pw (0.15f, 2.4f), .air = pw (0.30f, 1.6f),
      .barber = pw (0.35f, 1.6f), .noise = pw (0.45f, 1.7f),
      .delay = pw (0.40f, 1.5f), .feedback = pw (0.45f, 1.5f), .shift = pw (0.25f, 1.8f),
      .reverb = pw (0.50f, 1.4f), .comp = pw (0.30f, 1.5f), .drive = pw (0.15f, 2.2f),
      .delayFrom = 4, .delayTo = 8 },

    // Le preset par defaut : un build-up club complet.
    { .name = "Fist Pump",
      .highpass = pw (0.70f, 1.4f), .lowpass = pw (0.20f, 2.4f), .air = pw (0.45f, 1.5f),
      .pitch = pw (0.025f, 1.5f), .barber = pw (0.45f, 1.4f), .noise = pw (0.70f, 1.5f),
      .delay = pw (0.60f, 1.3f), .feedback = pw (0.60f, 1.3f), .shift = pw (0.35f, 1.6f),
      .gate = pw (0.35f, 2.2f), .reverb = pw (0.55f, 1.3f), .comp = pw (0.50f, 1.3f),
      .drive = pw (0.30f, 1.8f),
      .delayFrom = 4, .delayTo = 16, .pitchWet = 0.4f },

    // Le ballon gonfle : tout le morceau monte de 2 demi-tons (effet bande).
    { .name = "Balloon Head",
      .highpass = pw (0.55f, 1.6f), .lowpass = pw (0.15f, 2.5f), .air = pw (0.50f, 1.4f),
      .pitch = pw (0.17f, 1.3f), .barber = pw (0.65f, 1.3f), .noise = pw (0.55f, 1.6f),
      .delay = pw (0.45f, 1.5f), .feedback = pw (0.55f, 1.4f), .shift = pw (0.55f, 1.4f),
      .reverb = pw (0.70f, 1.2f), .comp = pw (0.45f, 1.4f), .drive = pw (0.20f, 2.0f),
      .delayFrom = 4, .delayTo = 16, .pitchWet = 1.0f },

    // Le haut se referme aussi : on entre dans un tunnel de reverbe.
    { .name = "Tunnel Vision",
      .highpass = pw (0.80f, 1.2f), .lowpass = pw (0.60f, 1.4f),
      .barber = pw (0.70f, 1.2f), .noise = pw (0.60f, 1.6f),
      .delay = pw (0.55f, 1.3f), .feedback = pw (0.65f, 1.2f), .shift = pw (0.30f, 1.8f),
      .reverb = pw (0.90f, 1.1f), .comp = pw (0.50f, 1.4f), .drive = pw (0.25f, 2.0f),
      .delayFrom = 8, .delayTo = 16 },

    // Le barber pole au maximum : spirale ascendante permanente.
    { .name = "Barber Pole",
      .highpass = pw (0.55f, 1.6f), .lowpass = pw (0.20f, 2.4f), .air = pw (0.40f, 1.5f),
      .pitch = pw (0.025f, 1.2f), .barber = pw (0.95f, 1.0f), .noise = pw (0.40f, 1.8f),
      .delay = pw (0.70f, 1.2f), .feedback = pw (0.75f, 1.1f), .shift = pw (0.90f, 1.0f),
      .reverb = pw (0.65f, 1.2f), .comp = pw (0.45f, 1.4f), .drive = pw (0.20f, 2.0f),
      .delayFrom = 4, .delayTo = 16, .pitchWet = 0.4f },

    // Le souffle a fond, et une octave qui decolle tard (courbe exponentielle).
    { .name = "Rocket Fuel",
      .highpass = pw (0.75f, 1.3f), .lowpass = pw (0.25f, 2.3f), .air = pw (0.55f, 1.4f),
      .pitch = ex (1.0f, 3.5f, 0.30f), .barber = pw (0.50f, 1.5f), .noise = pw (1.00f, 1.3f),
      .delay = pw (0.50f, 1.4f), .feedback = pw (0.55f, 1.4f), .shift = pw (0.35f, 1.7f),
      .gate = pw (0.45f, 2.0f), .reverb = pw (0.60f, 1.3f), .comp = pw (0.65f, 1.3f),
      .drive = pw (0.50f, 1.5f),
      .delayFrom = 8, .delayTo = 16, .pitchWet = 0.3f },

    { .name = "Jaw Drop",
      .highpass = pw (0.85f, 1.1f), .lowpass = pw (0.45f, 1.7f), .air = pw (0.60f, 1.3f),
      .pitch = pw (0.03f, 1.5f), .barber = pw (0.75f, 1.2f), .noise = pw (0.85f, 1.4f),
      .delay = pw (0.70f, 1.2f), .feedback = pw (0.70f, 1.2f), .shift = pw (0.50f, 1.5f),
      .gate = pw (0.70f, 1.6f), .reverb = pw (0.70f, 1.2f), .comp = pw (0.75f, 1.2f),
      .drive = pw (0.70f, 1.3f),
      .delayFrom = 8, .delayTo = 16, .pitchWet = 0.4f },

    // Tout au maximum ; une octave monte en courbe en S par-dessus.
    { .name = "Meltdown",
      .highpass = pw (0.92f, 1.0f), .lowpass = pw (0.65f, 1.3f), .air = pw (0.45f, 1.2f),
      .pitch = sc (1.0f, 1.6f, 0.20f), .barber = pw (0.90f, 1.0f), .noise = pw (1.00f, 1.2f),
      .delay = pw (0.80f, 1.1f), .feedback = pw (0.80f, 1.1f), .shift = pw (0.70f, 1.2f),
      .gate = pw (0.85f, 1.3f), .reverb = pw (0.85f, 1.1f), .comp = pw (0.90f, 1.1f),
      .drive = pw (0.90f, 1.1f),
      .delayFrom = 8, .delayTo = 32, .pitchWet = 0.4f },

    //==========================================================================
    //  Nouveaux en v0.6
    //==========================================================================

    // Tout le mix monte d'une octave avec le potard (doublage 80 % mouille).
    { .name = "Helium",
      .highpass = sc (0.62f, 2.0f), .air = pw (0.50f, 1.4f),
      .pitch = pw (1.00f, 1.2f), .noise = pw (0.35f, 1.6f),
      .delay = pw (0.35f, 1.5f), .feedback = pw (0.40f, 1.4f),
      .reverb = pw (0.60f, 1.3f), .comp = pw (0.50f, 1.4f), .drive = pw (0.15f, 2.0f),
      .delayFrom = 8, .delayTo = 16, .pitchWet = 0.8f },

    // Espace immense et brillant, aucune agressivite : pas de drive, pas de gate.
    { .name = "Cloud Nine",
      .highpass = sc (0.55f, 2.2f), .air = sc (0.70f, 1.8f),
      .pitch = pw (0.025f, 1.2f), .barber = pw (0.30f, 1.5f), .noise = pw (0.30f, 1.8f),
      .delay = pw (0.45f, 1.4f), .feedback = pw (0.60f, 1.3f), .shift = pw (0.20f, 1.6f),
      .reverb = sc (0.95f, 1.6f), .comp = pw (0.35f, 1.5f),
      .delayFrom = 4, .delayTo = 8, .pitchWet = 0.3f },

    // Courbes en S avec seuils : presque rien jusqu'a 30-40 %, puis tout eclot.
    { .name = "Goosebumps",
      .highpass = sc (0.72f, 3.0f, 0.25f), .air = sc (0.60f, 2.5f, 0.30f),
      .pitch = sc (1.00f, 2.5f, 0.50f), .barber = sc (0.60f, 2.5f, 0.30f),
      .noise = sc (0.70f, 2.5f, 0.35f),
      .delay = sc (0.60f, 2.5f, 0.40f), .feedback = sc (0.65f, 2.0f, 0.40f),
      .shift = sc (0.40f, 2.0f, 0.50f), .gate = sc (0.50f, 2.5f, 0.65f),
      .reverb = sc (0.80f, 3.0f, 0.30f), .comp = sc (0.70f, 2.0f, 0.30f),
      .drive = sc (0.45f, 2.5f, 0.50f),
      .delayFrom = 8, .delayTo = 16, .pitchWet = 0.3f },

    // Rythmique : gate et delay qui accelerent jusqu'a la triple croche.
    { .name = "Countdown",
      .highpass = ex (0.78f, 2.5f), .air = ex (0.60f, 2.0f),
      .barber = pw (0.40f, 1.5f), .noise = ex (0.90f, 2.5f),
      .delay = pw (0.60f, 1.2f), .feedback = pw (0.70f, 1.2f), .shift = pw (0.30f, 1.5f),
      .gate = pw (0.90f, 1.2f, 0.15f), .reverb = pw (0.50f, 1.4f), .comp = pw (0.80f, 1.2f),
      .drive = ex (0.55f, 2.0f),
      .delayFrom = 8, .delayTo = 32 }
};

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

inline constexpr int kNumPresets   = (int) (sizeof (kPresets) / sizeof (PresetDef));
inline constexpr int kDefaultPreset = 1;   // Fist Pump

inline juce::StringArray getPresetNames()
{
    juce::StringArray names;
    for (const auto& p : kPresets)
        names.add (p.name);
    return names;
}

// Valeur normalisee (0..1) du module 'index' pour la position t du potard.
inline float modValue (const PresetDef& p, int index, float t) noexcept
{
    return skygrin::evaluate (p.*kCurveOf[(size_t) index], t);
}

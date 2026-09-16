#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// ============================================================================
//  Subshaper — paramètres et utilitaires musicaux
// ============================================================================
namespace params
{
inline const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// Fréquence d'une note (0 = C) à une octave donnée (C1 = 32,70 Hz)
inline float noteFrequency (int note, int octave)
{
    const int midi = 12 * (octave + 1) + note;
    return (float) (440.0 * std::pow (2.0, (midi - 69) / 12.0));
}

// Nom de note + écart en cents pour une fréquence
inline juce::String noteNameForFrequency (float hz, float* centsOut = nullptr, int* midiOut = nullptr)
{
    if (hz <= 0.0f) return "-";
    const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
    const int nearest = (int) std::lround (midi);
    if (centsOut != nullptr) *centsOut = (float) ((midi - nearest) * 100.0);
    if (midiOut != nullptr)  *midiOut = nearest;
    const int note = ((nearest % 12) + 12) % 12;
    const int oct  = nearest / 12 - 1;
    return noteNames[note] + juce::String (oct);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [&] (const char* id, const char* name, float def, float lo = 0.0f, float hi = 100.0f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
            NormalisableRange<float> (lo, hi, 0.1f), def, AudioParameterFloatAttributes().withLabel ("%")));
    };
    auto db = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
            NormalisableRange<float> (-18.0f, 18.0f, 0.1f), def, AudioParameterFloatAttributes().withLabel ("dB")));
    };
    auto flag = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto choice = [&] (const char* id, const char* name, const StringArray& items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    // --- Global ---
    db ("inGain", "Input", 0.0f);
    NormalisableRange<float> xover (40.0f, 400.0f, 0.1f);
    xover.setSkewForCentre (120.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "crossover", 1 }, "Crossover", xover, 120.0f,
                     AudioParameterFloatAttributes().withLabel ("Hz")));
    choice ("keyNote", "Key", noteNames, 0);
    choice ("keyOct", "Key Octave", StringArray { "0", "1", "2", "3" }, 1);
    pct ("mix", "Mix", 100.0f);
    db ("output", "Output", 0.0f);
    flag ("monoLow", "Mono Low", true);
    flag ("soloLow", "Solo Low", false);
    flag ("subCut", "Sub Cut", true);
    flag ("delta", "Delta", false);
    flag ("gainMatch", "Gain Match", false);
    flag ("hq", "HQ Oversampling", true);

    // --- GEN : génération de sub ---
    flag ("genOn", "Gen On", false);
    choice ("genType", "Gen Type", StringArray { "Octave", "Sine" }, 0);
    pct ("genLevel", "Gen Level", 40.0f);
    pct ("genTone", "Gen Tone", 30.0f);
    flag ("keyLock", "Key Lock", false);

    // --- TONE : résonance accordée ---
    flag ("toneOn", "Tone On", false);
    pct ("toneAmt", "Tone Amount", 30.0f);
    pct ("toneQ", "Tone Q", 30.0f);
    choice ("toneHarm", "Tone Harmonic", StringArray { "1x Root", "2x Octave", "3x Fifth", "4x 2 Oct" }, 0);

    // --- DRIVE : saturation ---
    flag ("driveOn", "Drive On", false);
    choice ("driveType", "Drive Type", StringArray { "Tape", "Tube", "Hard", "Fold" }, 0);
    pct ("drive", "Drive", 35.0f);
    pct ("color", "Drive Color", 20.0f);
    pct ("focus", "Drive Focus", 0.0f);
    pct ("driveMix", "Drive Mix", 100.0f);

    // --- SHAPE : transitoires du grave ---
    flag ("shapeOn", "Shape On", false);
    pct ("attack", "Attack", 0.0f, -100.0f, 100.0f);
    pct ("sustain", "Sustain", 0.0f, -100.0f, 100.0f);

    // --- PUMP : ducking synchronisé au tempo ---
    flag ("pumpOn", "Pump On", false);
    choice ("pumpRate", "Pump Rate", StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" }, 2);
    pct ("pumpDepth", "Pump Depth", 50.0f);
    pct ("pumpShape", "Pump Release", 40.0f);

    // --- WIDTH : largeur au-dessus de la coupure ---
    flag ("widthOn", "Width On", false);
    pct ("width", "Width", 100.0f, 0.0f, 200.0f);

    return { p.begin(), p.end() };
}
} // namespace params

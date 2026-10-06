#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// ============================================================================
//  Velours — paramètres
// ============================================================================
namespace params
{
inline constexpr int numBands = 8;

enum BandType { bell = 0, lowShelf, highShelf, lowCut, highCut, bandPass, tilt, numBandTypes };
enum BandFocus { focusAll = 0, focusLeft, focusRight, focusMid, focusSide };

inline const juce::StringArray bandTypeNames  { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Band Pass", "Tilt" };
inline const juce::StringArray bandFocusNames { "All", "Left", "Right", "Mid", "Side" };
inline const juce::StringArray qualityNames   { "Low Latency", "Normal", "High Res" };

inline juce::String bandId (int band, const char* what) { return "b" + juce::String (band + 1) + "_" + what; }

struct BandDefault { bool on; int type; float freq, gain, q; };

// Réglage d'usine des 8 bandes : passe-haut, passe-bas et une cloche (comme soothe3)
inline BandDefault bandDefault (int i)
{
    static const BandDefault d[numBands] = {
        { true,  lowCut,    40.0f,   0.0f, 0.71f },
        { true,  bell,      2500.0f, 0.0f, 1.0f  },
        { true,  highCut,   18000.0f, 0.0f, 0.71f },
        { false, bell,      150.0f,  0.0f, 1.0f  },
        { false, bell,      500.0f,  0.0f, 1.0f  },
        { false, bell,      1000.0f, 0.0f, 1.0f  },
        { false, bell,      5000.0f, 0.0f, 1.0f  },
        { false, bell,      9000.0f, 0.0f, 1.0f  },
    };
    return d[juce::jlimit (0, numBands - 1, i)];
}

inline juce::NormalisableRange<float> logRange (float lo, float hi, float interval = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, interval);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}

inline juce::String freqText (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                         : juce::String (juce::roundToInt (hz)) + " Hz";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto fl = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, const String& label,
                   std::function<String (float, int)> toText = nullptr)
    {
        auto attr = AudioParameterFloatAttributes().withLabel (label);
        if (toText) attr = attr.withStringFromValueFunction (toText);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, attr));
    };
    auto flag = [&] (const String& id, const String& name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto choice = [&] (const String& id, const String& name, const StringArray& items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto oneDec = [] (float v, int) { return String (v, 1); };
    auto ms = [] (float v, int) { return v < 10.0f ? String (v, 1) : String (roundToInt (v)); };
    auto pctTxt = [] (float v, int) { return String (roundToInt (v)); };
    auto signedPct = [] (float v, int) { const int i = roundToInt (v); return (i > 0 ? "+" : "") + String (i); };
    auto signedDb = [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1); };

    // --- Cœur ---
    fl ("depth",   "Depth",   NormalisableRange<float> (0.0f, 20.0f, 0.01f), 5.0f, "", oneDec);
    fl ("detail",  "Detail",  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%", pctTxt);
    fl ("attack",  "Attack",  logRange (0.5f, 100.0f, 0.01f), 5.0f, "ms", ms);
    fl ("release", "Release", logRange (5.0f, 500.0f, 0.1f), 50.0f, "ms", ms);
    fl ("maxcut",  "Max Cut", NormalisableRange<float> (1.0f, 30.0f, 0.1f), 30.0f, "dB", oneDec);
    choice ("mode", "Mode", StringArray { "Soft", "Hard" }, 0);

    // --- Tilt (soothe3) ---
    fl ("detailTilt", "Detail Tilt", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%", signedPct);
    fl ("timeTilt",   "Time Tilt",   NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%", signedPct);

    // --- Stéréo / sidechain ---
    choice ("stereo", "Stereo Mode", StringArray { "L/R", "M/S" }, 0);
    fl ("link", "Stereo Link", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%", pctTxt);
    flag ("sidechain", "Sidechain", false);

    // --- Sortie ---
    fl ("mix",     "Mix",      NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%", pctTxt);
    fl ("wetTrim", "Wet Trim", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, "dB", signedDb);
    fl ("output",  "Output",   NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, "dB", signedDb);
    flag ("delta",  "Delta",  false);
    flag ("bypass", "Bypass", false);
    choice ("quality", "Resolution", qualityNames, 1);

    // --- Éditeur de bandes (sensibilité du traitement selon la fréquence) ---
    for (int b = 0; b < numBands; ++b)
    {
        const auto d = bandDefault (b);
        const String n = "Band " + String (b + 1) + " ";
        flag   (bandId (b, "on"),    n + "On", d.on);
        choice (bandId (b, "type"),  n + "Type", bandTypeNames, d.type);
        fl     (bandId (b, "freq"),  n + "Freq", logRange (20.0f, 20000.0f, 0.1f), d.freq, "Hz",
                [] (float v, int) { return freqText (v); });
        fl     (bandId (b, "gain"),  n + "Sensitivity", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), d.gain, "dB", signedDb);
        fl     (bandId (b, "q"),     n + "Q", logRange (0.2f, 10.0f, 0.01f), d.q, "",
                [] (float v, int) { return String (v, 2); });
        choice (bandId (b, "focus"), n + "Focus", bandFocusNames, focusAll);
    }

    return { p.begin(), p.end() };
}
} // namespace params

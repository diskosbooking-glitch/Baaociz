#pragma once
#include <vector>
#include "Params.h"

// ============================================================================
//  Velours — presets usine
// ============================================================================
namespace presets
{
struct BandSet { int slot; int type; float freq, gain, q; int focus = params::focusAll; };

struct Preset
{
    const char* name;
    float depth, detail, attack, release, maxCut;
    int mode;                 // 0 = Soft, 1 = Hard
    float detailTilt, attackTilt, releaseTilt;
    int stereo;               // 0 = L/R, 1 = M/S
    float link, mix;
    std::vector<BandSet> bands;   // les bandes non listées sont désactivées
};

using namespace params;

inline const std::vector<Preset>& factory()
{
    static const std::vector<Preset> list = {
        { "Init", 5.0f, 50.0f, 5.0f, 50.0f, 30.0f, 0, 0.0f, 0.0f, 0.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 40.0f, 0.0f, 0.71f }, { 1, bell, 2500.0f, 0.0f, 1.0f }, { 2, highCut, 18000.0f, 0.0f, 0.71f } } },

        { "Vocal - Smooth Harshness", 6.0f, 55.0f, 3.0f, 60.0f, 12.0f, 0, 10.0f, 20.0f, 20.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 180.0f, 0.0f, 0.71f }, { 1, bell, 3200.0f, 4.0f, 0.8f }, { 2, highCut, 16000.0f, 0.0f, 0.71f } } },

        { "Vocal - Surgical", 8.0f, 85.0f, 2.0f, 40.0f, 10.0f, 0, 0.0f, 0.0f, 0.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 250.0f, 0.0f, 0.71f }, { 1, bell, 2800.0f, 3.0f, 0.6f }, { 2, highCut, 14000.0f, 0.0f, 0.71f } } },

        { "Vocal - De-Ess", 10.0f, 40.0f, 1.0f, 40.0f, 15.0f, 1, 0.0f, 30.0f, 30.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 4500.0f, 0.0f, 0.71f }, { 1, bell, 7000.0f, 6.0f, 0.9f }, { 2, highCut, 16000.0f, 0.0f, 0.71f } } },

        { "Vocal - Mud Control", 5.0f, 60.0f, 8.0f, 90.0f, 8.0f, 0, -20.0f, -30.0f, -30.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 120.0f, 0.0f, 0.71f }, { 1, bell, 350.0f, 4.0f, 0.9f }, { 2, highCut, 1200.0f, 0.0f, 0.71f } } },

        { "Acoustic Guitar", 5.0f, 60.0f, 5.0f, 80.0f, 10.0f, 0, 0.0f, 0.0f, 0.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 100.0f, 0.0f, 0.71f }, { 1, bell, 220.0f, 3.0f, 1.0f }, { 2, highCut, 16000.0f, 0.0f, 0.71f },
            { 3, bell, 3500.0f, 2.0f, 0.8f } } },

        { "Electric Guitar Fizz", 7.0f, 45.0f, 4.0f, 70.0f, 12.0f, 0, 15.0f, 0.0f, 0.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 1800.0f, 0.0f, 0.71f }, { 1, bell, 5000.0f, 5.0f, 0.7f }, { 2, highCut, 15000.0f, 0.0f, 0.71f } } },

        { "Synth - Tame Resonance", 7.0f, 75.0f, 3.0f, 50.0f, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 150.0f, 0.0f, 0.71f }, { 1, bell, 2000.0f, 2.0f, 0.5f }, { 2, highCut, 18000.0f, 0.0f, 0.71f } } },

        { "808 / Bass - Boomy Notes", 6.0f, 70.0f, 10.0f, 150.0f, 9.0f, 0, -30.0f, -40.0f, -40.0f, 1, 100.0f, 100.0f,
          { { 0, lowCut, 30.0f, 0.0f, 0.71f }, { 1, bell, 90.0f, 3.0f, 1.0f }, { 2, highCut, 600.0f, 0.0f, 0.71f } } },

        { "Drum Bus - Ring", 6.0f, 80.0f, 10.0f, 120.0f, 10.0f, 0, 0.0f, 0.0f, 0.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 100.0f, 0.0f, 0.71f }, { 1, bell, 400.0f, 3.0f, 1.2f }, { 2, highCut, 12000.0f, 0.0f, 0.71f } } },

        { "Cymbals - Harsh Hats", 8.0f, 50.0f, 1.5f, 60.0f, 12.0f, 1, 0.0f, 40.0f, 40.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 3000.0f, 0.0f, 0.71f }, { 1, bell, 6000.0f, 3.0f, 0.7f }, { 2, highCut, 19000.0f, 0.0f, 0.71f } } },

        { "Piano - Ringing Notes", 4.0f, 80.0f, 15.0f, 200.0f, 8.0f, 0, 0.0f, -20.0f, -20.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 60.0f, 0.0f, 0.71f }, { 1, bell, 1500.0f, 0.0f, 1.0f }, { 2, highCut, 12000.0f, 0.0f, 0.71f } } },

        { "Mix Bus - Gentle", 3.0f, 35.0f, 8.0f, 150.0f, 6.0f, 0, 0.0f, 0.0f, 0.0f, 0, 100.0f, 70.0f,
          { { 0, lowCut, 120.0f, 0.0f, 0.71f }, { 1, bell, 3000.0f, 2.0f, 0.6f }, { 2, highCut, 16000.0f, 0.0f, 0.71f } } },

        { "Master - Air Polish", 2.5f, 30.0f, 10.0f, 200.0f, 4.0f, 0, 0.0f, 0.0f, 0.0f, 1, 100.0f, 100.0f,
          { { 0, lowCut, 2500.0f, 0.0f, 0.71f }, { 2, highCut, 18000.0f, 0.0f, 0.71f }, { 3, highShelf, 8000.0f, 3.0f, 0.71f },
            { 4, bell, 4000.0f, 0.0f, 1.0f, focusSide } } },

        { "Stereo - Wide Side Harsh", 6.0f, 55.0f, 4.0f, 60.0f, 10.0f, 0, 0.0f, 0.0f, 0.0f, 1, 0.0f, 100.0f,
          { { 0, lowCut, 1500.0f, 0.0f, 0.71f }, { 1, bell, 4000.0f, 4.0f, 0.8f, focusSide }, { 2, highCut, 16000.0f, 0.0f, 0.71f },
            { 3, bell, 4000.0f, -6.0f, 0.8f, focusMid } } },

        { "Heavy - Flatten Everything", 10.0f, 30.0f, 2.0f, 30.0f, 20.0f, 1, 0.0f, 0.0f, 0.0f, 0, 100.0f, 100.0f,
          { { 0, lowCut, 60.0f, 0.0f, 0.71f }, { 2, highCut, 18000.0f, 0.0f, 0.71f } } },
    };
    return list;
}
// Paramètres que les presets ne touchent pas
inline bool isProtected (const juce::String& id)
{
    return id == "bypass" || id == "quality" || id == "timeQuality" || id == "renderUltra" || id == "gainMatch";
}

inline juce::File userFolder()
{
   #if JUCE_MAC
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support/Velours/Presets");
   #else
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Velours/Presets");
   #endif
    dir.createDirectory();
    return dir;
}

inline juce::Array<juce::File> userPresets()
{
    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.velours");
    files.sort();
    return files;
}
} // namespace presets

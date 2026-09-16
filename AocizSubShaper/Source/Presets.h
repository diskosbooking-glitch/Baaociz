#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================================
//  Presets usine (valeurs en unités réelles ; booléens 0/1 ; choix = index)
//  Les paramètres absents reprennent leur valeur par défaut.
//  KEY, octave et qualité ne sont jamais modifiés par un preset.
// ============================================================================
namespace presets
{
struct Factory
{
    const char* name;
    const char* category;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Factory>& factory()
{
    static const std::vector<Factory> list = {
        { "Init", "Utility", {} },

        // --- Trap / rap US ---
        { "ATL Knock", "Trap", {
            { "crossover", 120 }, { "toneOn", 1 }, { "toneAmt", 35 }, { "toneQ", 20 }, { "toneHarm", 0 },
            { "driveOn", 1 }, { "driveType", 0 }, { "drive", 45 }, { "color", 25 }, { "focus", 50 }, { "driveMix", 70 },
            { "shapeOn", 1 }, { "attack", 30 } } },
        { "Crushed 808", "Trap", {
            { "crossover", 150 }, { "toneOn", 1 }, { "toneAmt", 20 }, { "toneHarm", 1 },
            { "driveOn", 1 }, { "driveType", 2 }, { "drive", 80 }, { "color", 40 }, { "focus", 30 }, { "driveMix", 85 },
            { "shapeOn", 1 }, { "sustain", -20 } } },
        { "Drill Glide", "Trap", {
            { "crossover", 130 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 45 }, { "genTone", 30 }, { "keyLock", 0 },
            { "driveOn", 1 }, { "driveType", 1 }, { "drive", 55 }, { "color", 50 }, { "focus", 40 }, { "driveMix", 60 },
            { "shapeOn", 1 }, { "sustain", 25 } } },
        { "R&B Clean Sub", "Trap", {
            { "crossover", 100 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 60 }, { "genTone", 70 }, { "keyLock", 1 },
            { "driveOn", 1 }, { "driveType", 0 }, { "drive", 15 }, { "driveMix", 40 } } },
        { "Phone Speaker Rescue", "Trap", {
            { "crossover", 140 }, { "toneOn", 1 }, { "toneAmt", 25 }, { "toneHarm", 1 },
            { "driveOn", 1 }, { "driveType", 1 }, { "drive", 60 }, { "color", 60 }, { "focus", 70 }, { "driveMix", 50 } } },

        // --- House ---
        { "Deep House Sine", "House", {
            { "crossover", 100 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 55 }, { "genTone", 80 }, { "keyLock", 1 },
            { "pumpOn", 1 }, { "pumpRate", 2 }, { "pumpDepth", 60 }, { "pumpShape", 40 },
            { "widthOn", 1 }, { "width", 130 } } },
        { "Tech House Roll", "House", {
            { "crossover", 110 }, { "toneOn", 1 }, { "toneAmt", 40 }, { "toneQ", 35 },
            { "driveOn", 1 }, { "driveType", 0 }, { "drive", 30 }, { "driveMix", 50 },
            { "shapeOn", 1 }, { "attack", 20 }, { "sustain", -30 },
            { "pumpOn", 1 }, { "pumpRate", 2 }, { "pumpDepth", 45 }, { "pumpShape", 30 } } },
        { "Bass House Growl", "House", {
            { "crossover", 180 }, { "genOn", 1 }, { "genType", 0 }, { "genLevel", 30 }, { "genTone", 40 },
            { "driveOn", 1 }, { "driveType", 3 }, { "drive", 55 }, { "color", 35 }, { "focus", 30 }, { "driveMix", 70 },
            { "pumpOn", 1 }, { "pumpRate", 2 }, { "pumpDepth", 50 },
            { "widthOn", 1 }, { "width", 140 } } },
        { "Garage Wobble Sub", "House", {
            { "crossover", 120 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 50 }, { "genTone", 50 },
            { "driveOn", 1 }, { "driveType", 1 }, { "drive", 25 }, { "driveMix", 50 },
            { "pumpOn", 1 }, { "pumpRate", 3 }, { "pumpDepth", 45 }, { "pumpShape", 55 } } },

        // --- Utilitaires ---
        { "Mono Fix", "Utility", { { "crossover", 150 }, { "monoLow", 1 }, { "subCut", 1 } } },
        { "Sub Tighten", "Utility", { { "crossover", 90 }, { "subCut", 1 }, { "shapeOn", 1 }, { "sustain", -45 } } },
    };
    return list;
}

inline bool isProtected (const juce::String& id)
{
    return id == "keyNote" || id == "keyOct" || id == "hq";
}

inline void apply (juce::AudioProcessorValueTreeState& apvts, const Factory& preset)
{
    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || isProtected (rp->getParameterID()))
            continue;

        float target = rp->getDefaultValue();
        for (auto& [id, value] : preset.values)
            if (rp->getParameterID() == id)
                target = rp->convertTo0to1 (value);

        rp->beginChangeGesture();
        rp->setValueNotifyingHost (target);
        rp->endChangeGesture();
    }
}

inline juce::File userFolder()
{
   #if JUCE_MAC
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support/Subshaper/Presets");
   #else
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Subshaper/Presets");
   #endif
    dir.createDirectory();
    return dir;
}

inline juce::Array<juce::File> userPresets()
{
    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.ssp");
    files.sort();
    return files;
}
} // namespace presets

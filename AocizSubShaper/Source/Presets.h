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
        { "Init", "", {} },

        // --- Trap ---
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
        { "NY Drill Punch", "Trap", {
            { "crossover", 140 }, { "driveOn", 1 }, { "driveType", 2 }, { "drive", 60 }, { "color", 30 }, { "focus", 45 }, { "driveMix", 75 },
            { "shapeOn", 1 }, { "attack", 45 }, { "sustain", 15 } } },
        { "Memphis Rumble", "Trap", {
            { "crossover", 110 }, { "genOn", 1 }, { "genType", 0 }, { "genLevel", 35 }, { "genTone", 25 },
            { "driveOn", 1 }, { "driveType", 1 }, { "drive", 40 }, { "color", 35 }, { "driveMix", 60 },
            { "shapeOn", 1 }, { "sustain", 35 } } },
        { "Rage Distorted", "Trap", {
            { "crossover", 180 }, { "toneOn", 1 }, { "toneAmt", 30 }, { "toneHarm", 1 },
            { "driveOn", 1 }, { "driveType", 3 }, { "drive", 60 }, { "color", 50 }, { "focus", 35 }, { "driveMix", 75 },
            { "widthOn", 1 }, { "width", 150 } } },
        { "Plugg Soft 808", "Trap", {
            { "crossover", 100 }, { "toneOn", 1 }, { "toneAmt", 25 }, { "toneQ", 15 },
            { "driveOn", 1 }, { "driveType", 0 }, { "drive", 20 }, { "driveMix", 45 },
            { "shapeOn", 1 }, { "attack", -20 }, { "sustain", 20 } } },
        { "Phone Speaker Rescue", "Trap", {
            { "crossover", 140 }, { "toneOn", 1 }, { "toneAmt", 25 }, { "toneHarm", 1 },
            { "driveOn", 1 }, { "driveType", 1 }, { "drive", 60 }, { "color", 60 }, { "focus", 70 }, { "driveMix", 50 } } },

        // --- Rap / R&B ---
        { "Boom Bap Warm", "Rap & R&B", {
            { "crossover", 90 }, { "toneOn", 1 }, { "toneAmt", 20 }, { "toneQ", 10 },
            { "driveOn", 1 }, { "driveType", 1 }, { "drive", 30 }, { "color", 45 }, { "driveMix", 40 },
            { "shapeOn", 1 }, { "attack", 25 }, { "sustain", -15 } } },
        { "R&B Clean Sub", "Rap & R&B", {
            { "crossover", 100 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 60 }, { "genTone", 70 }, { "keyLock", 1 },
            { "driveOn", 1 }, { "driveType", 0 }, { "drive", 15 }, { "driveMix", 40 } } },
        { "West Coast Bounce", "Rap & R&B", {
            { "crossover", 120 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 40 }, { "genTone", 40 },
            { "toneOn", 1 }, { "toneAmt", 30 }, { "toneHarm", 1 }, { "toneQ", 40 },
            { "shapeOn", 1 }, { "attack", 20 }, { "sustain", -25 } } },

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
        { "Minimal Rubber", "House", {
            { "crossover", 130 }, { "toneOn", 1 }, { "toneAmt", 45 }, { "toneQ", 60 },
            { "shapeOn", 1 }, { "attack", 35 }, { "sustain", -40 },
            { "pumpOn", 1 }, { "pumpRate", 3 }, { "pumpDepth", 25 }, { "pumpShape", 50 } } },
        { "Garage Wobble Sub", "House", {
            { "crossover", 120 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 50 }, { "genTone", 50 },
            { "driveOn", 1 }, { "driveType", 1 }, { "drive", 25 }, { "driveMix", 50 },
            { "pumpOn", 1 }, { "pumpRate", 3 }, { "pumpDepth", 45 }, { "pumpShape", 55 } } },

        // --- Techno ---
        { "Techno Rumble", "Techno", {
            { "crossover", 90 }, { "driveOn", 1 }, { "driveType", 1 }, { "drive", 50 }, { "color", 30 }, { "driveMix", 70 },
            { "shapeOn", 1 }, { "sustain", 40 },
            { "pumpOn", 1 }, { "pumpRate", 2 }, { "pumpDepth", 75 }, { "pumpShape", 55 } } },
        { "Melodic Techno Sub", "Techno", {
            { "crossover", 110 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 50 }, { "genTone", 60 }, { "keyLock", 1 },
            { "pumpOn", 1 }, { "pumpRate", 2 }, { "pumpDepth", 55 }, { "pumpShape", 70 },
            { "widthOn", 1 }, { "width", 150 } } },

        // --- Afro / Latin ---
        { "Afro House Round", "Afro & Latin", {
            { "crossover", 110 }, { "genOn", 1 }, { "genType", 1 }, { "genLevel", 40 }, { "genTone", 50 },
            { "driveOn", 1 }, { "driveType", 0 }, { "drive", 20 }, { "driveMix", 40 },
            { "pumpOn", 1 }, { "pumpRate", 2 }, { "pumpDepth", 30 }, { "pumpShape", 45 } } },
        { "Amapiano Log Drum", "Afro & Latin", {
            { "crossover", 150 }, { "toneOn", 1 }, { "toneAmt", 50 }, { "toneQ", 55 }, { "toneHarm", 0 },
            { "driveOn", 1 }, { "driveType", 0 }, { "drive", 35 }, { "focus", 40 }, { "driveMix", 60 },
            { "shapeOn", 1 }, { "attack", 40 }, { "sustain", 20 } } },
        { "Dembow Punch", "Afro & Latin", {
            { "crossover", 120 }, { "driveOn", 1 }, { "driveType", 0 }, { "drive", 35 }, { "driveMix", 55 },
            { "shapeOn", 1 }, { "attack", 35 }, { "sustain", -20 } } },

        // --- Utilitaires ---
        { "Mono Fix", "Utility", { { "crossover", 150 }, { "monoLow", 1 }, { "subCut", 1 } } },
        { "Sub Tighten", "Utility", { { "crossover", 90 }, { "subCut", 1 }, { "shapeOn", 1 }, { "sustain", -45 } } },
        { "Kick Space", "Utility", { { "crossover", 110 }, { "pumpOn", 1 }, { "pumpRate", 2 }, { "pumpDepth", 70 }, { "pumpShape", 25 } } },
        { "Soft Glue", "Utility", { { "crossover", 120 }, { "driveOn", 1 }, { "driveType", 0 }, { "drive", 20 }, { "driveMix", 35 },
                                     { "shapeOn", 1 }, { "sustain", -10 } } },
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

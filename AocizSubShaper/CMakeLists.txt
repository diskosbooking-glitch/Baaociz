cmake_minimum_required(VERSION 3.22)
project(AocizSubShaper VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE : téléchargé automatiquement (ou dossier local via -DJUCE_DIR=...) ---
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# AU uniquement sur Mac ; VST3 partout
if(APPLE)
    set(SUBSHAPER_FORMATS AU VST3 Standalone)
else()
    set(SUBSHAPER_FORMATS VST3 Standalone)
endif()

option(SUBSHAPER_COPY_AFTER_BUILD "Copier le plugin dans les dossiers système après compilation" OFF)

juce_add_plugin(AocizSubShaper
    COMPANY_NAME                "Aociz"
    BUNDLE_ID                   "com.aociz.subshaper"
    PLUGIN_MANUFACTURER_CODE    Aocz
    PLUGIN_CODE                 SbSh
    PRODUCT_NAME                "Aociz SubShaper"
    FORMATS                     ${SUBSHAPER_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Distortion
    COPY_PLUGIN_AFTER_BUILD     ${SUBSHAPER_COPY_AFTER_BUILD})

target_sources(AocizSubShaper PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AocizSubShaper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AocizSubShaper
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

cmake_minimum_required(VERSION 3.22)

# ---- Numero de version du plugin : incrementer a chaque mise a jour ----
project(Velours VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE : telecharge automatiquement (ou dossier local via -DJUCE_DIR=...) ---
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
    set(VELOURS_FORMATS AU VST3)
else()
    set(VELOURS_FORMATS VST3)
endif()

juce_add_plugin(Velours
    COMPANY_NAME                "Aociz"
    BUNDLE_ID                   "com.aociz.velours"
    PLUGIN_MANUFACTURER_CODE    Aocz
    PLUGIN_CODE                 Vlrs
    PRODUCT_NAME                "Velours"
    VERSION                     ${PROJECT_VERSION}
    FORMATS                     ${VELOURS_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx EQ Dynamics
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(Velours PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Velours PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Velours
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Memento — instrument AU génératif (assembleur de "song starters" façon
# Upcycle) bâti avec JUCE. v0.1 : scan d'un dossier de samples, slots par rôle,
# reroll/lock, calage au tempo de l'hôte (time-stretch SoundTouch), lecture
# multi-voix synchronisée, vol/mute/solo par slot.
# ---------------------------------------------------------------------------

# Le numéro de version du plugin vit ICI (comme pour Subshaper/Skygrin).
project(Memento VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()
# Universel (Apple Silicon + Intel) comme un vrai plugin distribuable.
if(NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

include(FetchContent)

# --- JUCE ------------------------------------------------------------------
FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        7.0.12
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# v0.1 : calage au tempo par ré-échantillonnage intégré (aucune dépendance
# externe). Le time-stretch à hauteur préservée (SoundTouch) reviendra en v0.2.

# ---------------------------------------------------------------------------
juce_add_plugin(Memento
  PLUGIN_MANUFACTURER_CODE Aciz
  PLUGIN_CODE              Mmto
  COMPANY_NAME             "Aociz"
  BUNDLE_ID                com.aociz.memento
  IS_SYNTH                 TRUE
  NEEDS_MIDI_INPUT         FALSE
  NEEDS_MIDI_OUTPUT        FALSE
  IS_MIDI_EFFECT           FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD  FALSE
  FORMATS                  AU
  PRODUCT_NAME             "Memento"
)

# Génère <JuceHeader.h> (utilisé par les sources).
juce_generate_juce_header(Memento)

target_sources(Memento PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/MementoEngine.cpp
)

target_compile_definitions(Memento PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Memento PRIVATE
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)

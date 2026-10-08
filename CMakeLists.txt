cmake_minimum_required(VERSION 3.22)
project(MC909Editor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary so the AU loads in both Intel and Apple Silicon hosts.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MC909Editor
    COMPANY_NAME              "Independent"
    PLUGIN_MANUFACTURER_CODE  Indp
    PLUGIN_CODE               M909
    FORMATS                   AU VST3 Standalone
    PRODUCT_NAME              "MC-909 Editor"
    BUNDLE_ID                 com.example.mc909editor

    # Audio passes straight through; the plugin exists to drive the hardware.
    IS_SYNTH                  FALSE
    IS_MIDI_EFFECT            FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   TRUE
)

target_sources(MC909Editor PRIVATE
    Source/RolandSysEx.cpp
    Source/MC909Map.cpp
    Source/MidiHub.cpp
    Source/PluginProcessor.cpp
    Source/UiKit.cpp
    Source/Pages.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(MC909Editor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(MC909Editor PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_processors
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

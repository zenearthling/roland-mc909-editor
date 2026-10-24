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

set(MC909_SOURCES
    Source/RolandSysEx.cpp
    Source/MC909Map.cpp
    Source/MidiHub.cpp
    Source/PluginProcessor.cpp
    Source/UiKit.cpp
    Source/Pages.cpp
    Source/PluginEditor.cpp
    Source/Help.cpp
    Source/MasterPage.cpp
    Source/CompEqPage.cpp
    Source/PartMixerPage.cpp
    Source/AudioAnalyser.cpp
    Source/FxData.cpp
    Source/SearchIndex.cpp
    Source/SearchData.cpp
    Source/SearchOverlay.cpp
)

# Two builds of the same code:
#   MC909Editor            audio effect. Goes on the audio track that carries the unit's audio
#                          (the Master tab's meters and spectrum need that audio).
#   MC909EditorInstrument  instrument. Goes on a MIDI track; Live can then route other MIDI
#                          tracks and the MPK to it, so the keyboard and pads light up for those
#                          notes and (with Thru on) they are passed on to the MC-909.
function(mc909_add_plugin target product code bundle formats synth)
    juce_add_plugin(${target}
        COMPANY_NAME              "Independent"
        PLUGIN_MANUFACTURER_CODE  Indp
        PLUGIN_CODE               ${code}
        FORMATS                   ${formats}
        PRODUCT_NAME              "${product}"
        BUNDLE_ID                 ${bundle}

        IS_SYNTH                  ${synth}
        IS_MIDI_EFFECT            FALSE
        NEEDS_MIDI_INPUT          TRUE
        NEEDS_MIDI_OUTPUT         TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD   TRUE
    )

    target_sources(${target} PRIVATE ${MC909_SOURCES})

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    if(synth)
        target_compile_definitions(${target} PUBLIC MC909_INSTRUMENT=1)
    endif()

    target_link_libraries(${target} PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endfunction()

mc909_add_plugin(MC909Editor "MC-909 Editor" M909 com.example.mc909editor "AU;VST3;Standalone" FALSE)
mc909_add_plugin(MC909EditorInstrument "MC-909 Editor Instrument" M90I com.example.mc909editorinstrument "AU;VST3" TRUE)

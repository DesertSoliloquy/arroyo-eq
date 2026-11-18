cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0091 NEW)
project(ArroyoEQ VERSION 0.1.0)

# Static MSVC runtime: the plugin needs no Visual C++ redistributable on Windows
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: either a sibling checkout (JUCE_DIR) or fetched automatically.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(ARROYO_FORMATS VST3 Standalone)

juce_add_plugin(ArroyoEQ
    COMPANY_NAME "Arroyo"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.arroyo.arroyoeq"
    PLUGIN_MANUFACTURER_CODE Aryo
    PLUGIN_CODE AreQ
    FORMATS ${ARROYO_FORMATS}
    PRODUCT_NAME "Arroyo EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ Dynamics)

target_sources(ArroyoEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/gui/CurveDisplay.cpp)

target_include_directories(ArroyoEQ PRIVATE Source)

target_compile_definitions(ArroyoEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ArroyoEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(ARROYO_GENERIC_EDITOR "Diagnostic build: JUCE's generic editor instead of Arroyo's" OFF)
if(ARROYO_GENERIC_EDITOR)
    target_compile_definitions(ArroyoEQ PRIVATE ARROYO_GENERIC_EDITOR=1)
endif()

# Debug symbols in Release on Windows, so crash reports name functions.
# (The .pdb is kept out of the distributed zip.)
if(MSVC)
    target_compile_options(ArroyoEQ PRIVATE /Zi)
    target_link_options(ArroyoEQ PRIVATE /DEBUG /OPT:REF /OPT:ICF)
endif()

# Headless screenshot tool (development only)
option(ARROYO_BUILD_SNAPSHOT "Build the GUI snapshot tool" OFF)
if(ARROYO_BUILD_SNAPSHOT)
    juce_add_console_app(ArroyoSnapshot PRODUCT_NAME "ArroyoSnapshot")
    target_sources(ArroyoSnapshot PRIVATE
        tools/Snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/gui/CurveDisplay.cpp)
    target_include_directories(ArroyoSnapshot PRIVATE Source)
    target_compile_definitions(ArroyoSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ARROYO_NO_PLUGIN_ENTRY=1)
    target_link_libraries(ArroyoSnapshot PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
        juce::juce_recommended_config_flags)

    juce_add_console_app(ArroyoStress PRODUCT_NAME "ArroyoStress")
    target_sources(ArroyoStress PRIVATE
        tools/Stress.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/gui/CurveDisplay.cpp)
    target_include_directories(ArroyoStress PRIVATE Source)
    target_compile_definitions(ArroyoStress PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ARROYO_NO_PLUGIN_ENTRY=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ArroyoStress PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
        juce::juce_recommended_config_flags)
    if(MSVC)
        target_compile_options(ArroyoStress PRIVATE /Zi)
        target_link_options(ArroyoStress PRIVATE /DEBUG)
    endif()
endif()

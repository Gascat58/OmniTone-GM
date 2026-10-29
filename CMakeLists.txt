cmake_minimum_required(VERSION 3.22)

project(OmniToneGM VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # no extra runtime DLLs needed

option(OMNITONE_BUILD_PLUGIN "Build the OmniTone GM plugin" ON)
option(OMNITONE_BUILD_TESTS  "Build the command-line engine test" OFF)

# JUCE: use a local copy if given (-DOMNITONE_JUCE_PATH=...), otherwise download it.
if(OMNITONE_JUCE_PATH)
    add_subdirectory(${OMNITONE_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(OMNITONE_ENGINE_SOURCES
    Source/TsfImpl.cpp
    Source/SoundSet.cpp
    Source/SoundFontCache.cpp
    Source/OmniEngine.cpp)

if(OMNITONE_BUILD_PLUGIN)
    juce_add_plugin(OmniToneGM
        PRODUCT_NAME "OmniTone GM"
        COMPANY_NAME "OmniTone"
        BUNDLE_ID "com.omnitone.gm"
        PLUGIN_MANUFACTURER_CODE Omnt
        PLUGIN_CODE Otgm
        FORMATS VST3
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Instrument Synth)

    target_sources(OmniToneGM PRIVATE
        ${OMNITONE_ENGINE_SOURCES}
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(OmniToneGM PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(OmniToneGM
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(OMNITONE_BUILD_TESTS)
    juce_add_console_app(EngineTest PRODUCT_NAME "EngineTest")
    target_sources(EngineTest PRIVATE ${OMNITONE_ENGINE_SOURCES} test/EngineTest.cpp)
    target_compile_definitions(EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(EngineTest PRIVATE juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()

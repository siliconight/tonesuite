cmake_minimum_required(VERSION 3.22)

project(ToneSuite VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# macOS: build a universal binary (Apple Silicon + Intel) and target a
# deployment that covers any modern Logic Pro release. CMake reads these
# *before* the first project()/enable_language() call to take effect, which
# `juce_add_plugin` will trigger.
# ---------------------------------------------------------------------------
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    endif()
endif()

# ---------------------------------------------------------------------------
# Fetch JUCE. Pinned to a known-good tag.
# If you already have JUCE locally, replace this block with
#     add_subdirectory(path/to/JUCE)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin identity. CHANGE THESE if you're publishing - the 4-char codes
# below are not registered; for a release you should pick your own.
# ---------------------------------------------------------------------------
juce_add_plugin(ToneSuite
    COMPANY_NAME            "YourCompany"
    BUNDLE_ID               "com.yourcompany.tonesuite"
    PLUGIN_MANUFACTURER_CODE Yorc     # 4 chars, must contain >=1 uppercase
    PLUGIN_CODE             Pteq      # 4 chars, must contain >=1 uppercase
    FORMATS                 AU VST3 Standalone
    PRODUCT_NAME            "Tone Suite"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)     # auto-installs to ~/Library/Audio/Plug-Ins/{Components,VST3}/

target_sources(ToneSuite PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SpectrumView.cpp
    Source/VariableMuComp.h
    Source/DynamicEQ.h
    Source/TapeStage.h
    Source/Analyzer.h
    Source/SpectrumView.h
    Source/ResonanceSuppressor.h)

target_compile_definitions(ToneSuite PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ToneSuite PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# macOS code signing: ad-hoc sign every plugin target so Logic Pro accepts
# the AU without "untrusted developer" complaints in a personal build.
# Replace "-" with your Developer ID Application identity if you want to
# distribute outside your machine (and notarize, etc).
# ---------------------------------------------------------------------------
if(APPLE)
    foreach(fmt IN ITEMS AU VST3 Standalone)
        if(TARGET ToneSuite_${fmt})
            set_target_properties(ToneSuite_${fmt} PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE   "Automatic")
        endif()
    endforeach()
endif()

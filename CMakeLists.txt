cmake_minimum_required(VERSION 3.24)

project(VocalShiftAIStudio VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if (MSVC)
    # Static runtime: the .vst3 does not depend on the user having the VC++ redistributable.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(VS_BUILD_TESTS "Build unit / DSP tests" ON)
option(VS_BUILD_BENCH "Build the offline QualityBench renderer" ON)

include(cmake/Dependencies.cmake)

# ------------------------------------------------------------------------------
# vs_core: pure C++ DSP / analysis / state logic. No JUCE, testable in isolation.
# ------------------------------------------------------------------------------
add_library(vs_core STATIC
    src/core/VocalChain.cpp
    src/core/dsp/SignalsmithPitchShifter.cpp
    src/core/dsp/PsolaPitchShifter.cpp
    src/core/dsp/HybridPitchShifter.cpp
    src/core/mix/VocalMixEngine.cpp
    src/core/morph/ToneShaper.cpp
    src/core/analysis/PitchDetector.cpp
    src/core/state/FactoryPresets.cpp
    src/core/state/MacroMapping.cpp
    src/core/enhance/VocalEnhancer.cpp
    src/core/mix/LevelMatcher.cpp
    src/core/dsp/ConsonantBlend.cpp
    src/core/dsp/DeEsser.cpp
    src/core/morph/VoiceTexture.cpp
    src/core/space/SpatialEngine.cpp
)
target_include_directories(vs_core PUBLIC src)
target_link_libraries(vs_core PUBLIC signalsmith-stretch)
if (MSVC)
    target_compile_options(vs_core PRIVATE /bigobj /W4 /wd4100)
else()
    target_compile_options(vs_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
    # Signalsmith is very slow without optimisation; keep it optimised even in Debug.
    target_compile_options(vs_core PRIVATE $<$<CONFIG:Debug>:-O2>)
endif()

# ------------------------------------------------------------------------------
# Plugin (JUCE). Sources are shared via an INTERFACE target so tests/bench can
# instantiate the real AudioProcessor without going through a host.
# ------------------------------------------------------------------------------
add_library(vs_plugin_shared INTERFACE)
target_sources(vs_plugin_shared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/Parameters.cpp
)
target_compile_definitions(vs_plugin_shared INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    VS_VERSION_STRING="${PROJECT_VERSION}"
)
target_link_libraries(vs_plugin_shared INTERFACE
    vs_core
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_add_plugin(VocalShift
    PRODUCT_NAME "VocalShift AI Studio"
    COMPANY_NAME "VocalShift"
    BUNDLE_ID "com.vocalshift.aistudio"
    PLUGIN_MANUFACTURER_CODE Vshf
    PLUGIN_CODE Vsas
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Pitch Shift
    COPY_PLUGIN_AFTER_BUILD FALSE
)
target_sources(VocalShift PRIVATE src/plugin/PluginEntry.cpp)
target_link_libraries(VocalShift PRIVATE vs_plugin_shared)

if (VS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (VS_BUILD_BENCH)
    add_subdirectory(bench)
endif()

juce_add_console_app(vs_bench PRODUCT_NAME "VocalShiftBench")
target_sources(vs_bench PRIVATE QualityBench.cpp)
target_link_libraries(vs_bench PRIVATE vs_plugin_shared juce::juce_audio_formats)

juce_add_console_app(vs_lab PRODUCT_NAME "VocalShiftLab")
target_sources(vs_lab PRIVATE NaturalLab.cpp)
target_link_libraries(vs_lab PRIVATE vs_core juce::juce_audio_formats juce::juce_events)

execute_process(COMMAND git describe --always --dirty --abbrev=7 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE VS_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT VS_GIT_COMMIT)
    set(VS_GIT_COMMIT "unknown")
endif()
target_compile_definitions(vs_lab PRIVATE VS_GIT_COMMIT="${VS_GIT_COMMIT}")

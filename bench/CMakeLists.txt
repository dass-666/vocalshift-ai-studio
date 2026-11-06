juce_add_console_app(vs_bench PRODUCT_NAME "VocalShiftBench")
target_sources(vs_bench PRIVATE QualityBench.cpp)
target_link_libraries(vs_bench PRIVATE vs_plugin_shared juce::juce_audio_formats)

juce_add_console_app(vs_lab PRODUCT_NAME "VocalShiftLab")
target_sources(vs_lab PRIVATE NaturalLab.cpp)
target_link_libraries(vs_lab PRIVATE vs_core juce::juce_audio_formats juce::juce_events)

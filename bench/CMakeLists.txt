juce_add_console_app(vs_bench PRODUCT_NAME "VocalShiftBench")
target_sources(vs_bench PRIVATE QualityBench.cpp)
target_link_libraries(vs_bench PRIVATE vs_plugin_shared juce::juce_audio_formats)

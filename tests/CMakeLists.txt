add_executable(vs_core_tests CoreTests.cpp)
target_link_libraries(vs_core_tests PRIVATE vs_core)
add_test(NAME core_dsp_tests COMMAND vs_core_tests)

# Plugin-level tests: instantiate the real AudioProcessor (no host, no GUI).
juce_add_console_app(vs_plugin_tests PRODUCT_NAME "VocalShiftPluginTests")
target_sources(vs_plugin_tests PRIVATE PluginTests.cpp)
target_link_libraries(vs_plugin_tests PRIVATE vs_plugin_shared)
add_test(NAME plugin_tests COMMAND vs_plugin_tests)

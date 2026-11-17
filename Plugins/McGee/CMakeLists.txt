polar_plugin(McGee McG1 "McGEE — Lo-fi analog polysynth: ladder filter, wow & flutter, 12-bit crunch, haze, gated room" TRUE)
target_sources(McGee PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(McGee PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Aura/Source)

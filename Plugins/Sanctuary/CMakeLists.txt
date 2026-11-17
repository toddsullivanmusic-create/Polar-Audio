lumen_plugin(Sanctuary Snc1 "SANCTUARY — Live worship pad & drone engine with seamless key changes" TRUE)
target_sources(Sanctuary PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Sanctuary PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Aura/Source)

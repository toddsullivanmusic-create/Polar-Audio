polar_plugin(Prism Prs1 "PRISM — Four-module multi-effect: color, motion, space, air" FALSE)
target_sources(Prism PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Prism PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${CMAKE_CURRENT_SOURCE_DIR}/../Aura/Source)

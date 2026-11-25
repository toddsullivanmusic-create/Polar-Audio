polar_plugin(Prism Prs1 "PRISM — Four-module multi-effect: color, motion, space, air" FALSE)
target_sources(Prism PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Prism PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${CMAKE_CURRENT_SOURCE_DIR}/../Aura/Source)

# Embedded fonts (SIL OFL)
juce_add_binary_data(PrismFonts SOURCES
    Fonts/Michroma-Regular.ttf
    Fonts/BarlowCondensed-SemiBold.ttf
    Fonts/SpaceMono-Bold.ttf)
set_target_properties(PrismFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(Prism PRIVATE PrismFonts)

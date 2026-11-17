cmake_minimum_required(VERSION 3.22)
project(PolarAudio VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

# Formats: AU is what Logic Pro loads. VST3 + Standalone are built too (VST3 for other DAWs, Standalone for testing).
if(APPLE)
    set(POLAR_FORMATS AU VST3 Standalone)
else()
    set(POLAR_FORMATS VST3 Standalone)
endif()

# JUCE: use a checkout next to this folder if present, otherwise fetch it (CI does this).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Shared header-only lib (UI + DSP)
add_library(PolarShared INTERFACE)
target_include_directories(PolarShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Shared)

function(polar_plugin NAME CODE DESC IS_SYNTH)
    # JUCE derives AU_MAIN_TYPE / VST3_CATEGORIES from IS_SYNTH automatically.
    juce_add_plugin(${NAME}
        COMPANY_NAME "Polar Audio"
        BUNDLE_ID "com.polaraudio.${NAME}"
        PLUGIN_MANUFACTURER_CODE Polr
        PLUGIN_CODE ${CODE}
        FORMATS ${POLAR_FORMATS}
        PRODUCT_NAME "${NAME}"
        DESCRIPTION "${DESC}"
        IS_SYNTH ${IS_SYNTH}
        NEEDS_MIDI_INPUT ${IS_SYNTH}
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        HARDENED_RUNTIME_ENABLED TRUE)
    target_compile_definitions(${NAME} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(${NAME} PRIVATE
        PolarShared
        juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

foreach(P Aura Glow McGee Sanctuary)
    add_subdirectory(Plugins/${P})
endforeach()

option(POLAR_BUILD_TESTS "Build headless engine tests" ON)
if(POLAR_BUILD_TESTS)
    add_subdirectory(Tests)
endif()

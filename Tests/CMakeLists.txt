# Headless DSP verification: instantiates each processor, drives it with test signals,
# checks for NaN/inf/silence/clipping and writes a short WAV render for listening.
function(polar_test NAME PLUGIN)
    juce_add_console_app(${NAME} PRODUCT_NAME "${NAME}")
    target_sources(${NAME} PRIVATE EngineTest.cpp ../Plugins/${PLUGIN}/Source/PluginProcessor.cpp ../Plugins/${PLUGIN}/Source/PluginEditor.cpp)
    target_include_directories(${NAME} PRIVATE ../Plugins/${PLUGIN}/Source ../Plugins/Aura/Source ../Shared)
    target_compile_definitions(${NAME} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 POLAR_TEST_${PLUGIN}=1
        JucePlugin_Name="${PLUGIN}" JucePlugin_ManufacturerCode=0x506f6c72 JucePlugin_PluginCode=0x41757231 JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_EditorRequiresKeyboardFocus=0 JucePlugin_Manufacturer="Polar" JucePlugin_VersionString="1.0.0" JucePlugin_VersionCode=0x10000 JucePlugin_Desc="t" JucePlugin_AUMainType=0)
    target_link_libraries(${NAME} PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats juce::juce_recommended_config_flags)
endfunction()
polar_test(TestAura Aura)
polar_test(TestGlow Glow)
polar_test(TestMcGee McGee)
polar_test(TestSanctuary Sanctuary)
polar_test(TestPrism Prism)
target_link_libraries(TestPrism PRIVATE PrismFonts)

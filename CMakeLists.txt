cmake_minimum_required(VERSION 3.22)

project(VocalRider VERSION 1.1.1)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build Universal Binary on macOS (supports both Intel and Apple Silicon)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

# Add JUCE as a subdirectory
add_subdirectory(JUCE)

# Plugin sources
set(PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DSP/RMSDetector.cpp
    Source/DSP/RMSDetector.h
    Source/DSP/GainSmoother.cpp
    Source/DSP/GainSmoother.h
    Source/DSP/PeakDetector.cpp
    Source/DSP/PeakDetector.h
    Source/UI/LevelMeter.cpp
    Source/UI/LevelMeter.h
    Source/UI/CustomLookAndFeel.cpp
    Source/UI/CustomLookAndFeel.h
    Source/UI/WaveformDisplay.cpp
    Source/UI/WaveformDisplay.h
)

# Create the plugin target using juce_add_plugin
juce_add_plugin(VocalRider
    # Plugin metadata
    COMPANY_NAME "MBM Audio"
    BUNDLE_ID "com.mbmaudio.vocalrider"
    PLUGIN_MANUFACTURER_CODE Mbma
    PLUGIN_CODE Vrlr
    
    # Plugin formats to build
    FORMATS AU VST3 Standalone
    
    # Plugin name that appears in the DAW
    PRODUCT_NAME "magic.RIDE"
    
    # Plugin characteristics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    
    # Plugin categories
    VST3_CATEGORIES Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    
    # Copy plugin to system folder after build (optional, set to TRUE if desired)
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Add source files to the plugin target
target_sources(VocalRider PRIVATE ${PLUGIN_SOURCES})

# Set include directories
target_include_directories(VocalRider PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/UI
)

# Compile definitions for JUCE
target_compile_definitions(VocalRider
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Link JUCE modules
target_link_libraries(VocalRider
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ============================================================================
# Auto-install plugins to system folders after build (macOS only)
# ============================================================================
if(APPLE)
    # Define plugin destinations
    set(AU_DEST "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
    set(VST3_DEST "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
    
    # Custom target to install plugins
    add_custom_target(install_plugins
        COMMAND ${CMAKE_COMMAND} -E echo "Installing magic.RIDE plugins..."
        COMMAND ${CMAKE_COMMAND} -E make_directory "${AU_DEST}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_DEST}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${AU_DEST}/magic.RIDE.component"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${VST3_DEST}/magic.RIDE.vst3"
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
            "${CMAKE_BINARY_DIR}/VocalRider_artefacts/AU/magic.RIDE.component"
            "${AU_DEST}/magic.RIDE.component"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/VocalRider_artefacts/VST3/magic.RIDE.vst3"
            "${VST3_DEST}/magic.RIDE.vst3"
        COMMAND ${CMAKE_COMMAND} -E echo "✓ Plugins installed to system folders"
        COMMAND killall -9 AudioComponentRegistrar 2>/dev/null || true
        DEPENDS VocalRider_AU VocalRider_VST3
        COMMENT "Installing AU and VST3 plugins to system folders..."
    )
endif()
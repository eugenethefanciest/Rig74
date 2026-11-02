cmake_minimum_required(VERSION 3.22)

project(RIG74 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mac: build one universal binary (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# Windows: link the C runtime statically so the plugin works without extra installs
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(RIG74_BUILD_TESTS "Build the offline DSP/GUI test tool" OFF)
option(RIG74_MINIMAL_LINUX "Disable optional X11 extensions (for minimal Linux build boxes)" OFF)

# ---- JUCE -------------------------------------------------------------------
# Downloaded automatically. To use a local copy:  -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# ---- Plugin -----------------------------------------------------------------
set(RIG74_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RIG74_FORMATS AU)
endif()

juce_add_plugin(Rig74
    COMPANY_NAME "Karnaukh Audio"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.karnaukhaudio.rig74"
    PLUGIN_MANUFACTURER_CODE Krnh
    PLUGIN_CODE Rg74
    FORMATS ${RIG74_FORMATS}
    PRODUCT_NAME "Rig 74"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(Rig74Data
    HEADER_NAME Rig74BinaryData.h
    NAMESPACE Rig74BinaryData
    SOURCES
        Resources/fonts/Shrikhand-Regular.ttf
        Resources/fonts/BebasNeue-Regular.ttf)

set(RIG74_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/dsp/CabinetIRs.cpp
    Source/gui/RetroLookAndFeel.cpp
    Source/gui/Widgets.cpp)

target_sources(Rig74 PRIVATE ${RIG74_SOURCES})
target_include_directories(Rig74 PRIVATE Source)

set(RIG74_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(RIG74_MINIMAL_LINUX)
    list(APPEND RIG74_DEFINES
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
        JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_compile_definitions(Rig74 PUBLIC ${RIG74_DEFINES})

target_link_libraries(Rig74
    PRIVATE
        Rig74Data
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Offline test tool (renders audio through the chain + GUI screenshot) ----
if(RIG74_BUILD_TESTS)
    juce_add_console_app(Rig74Test PRODUCT_NAME "Rig74Test")
    target_sources(Rig74Test PRIVATE tools/OfflineTest.cpp ${RIG74_SOURCES})
    target_include_directories(Rig74Test PRIVATE Source)
    target_compile_definitions(Rig74Test PRIVATE ${RIG74_DEFINES} RIG74_TEST_BUILD=1)
    target_link_libraries(Rig74Test PRIVATE
        Rig74Data juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)

    juce_add_console_app(Rig74HostTest PRODUCT_NAME "Rig74HostTest")
    target_sources(Rig74HostTest PRIVATE tools/HostTest.cpp)
    target_compile_definitions(Rig74HostTest PRIVATE ${RIG74_DEFINES} JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(Rig74HostTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()

cmake_minimum_required(VERSION 3.22)
project(OneDial VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies via CPM ---
include(cmake/CPM.cmake)

# JUCE
CPMAddPackage(
    NAME JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    OPTIONS
        "JUCE_BUILD_EXAMPLES OFF"
        "JUCE_BUILD_EXTRAS OFF"
)

# NeuralAmpModelerCore
CPMAddPackage(
    NAME NeuralAmpModelerCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG main
)

# Eigen (required by NeuralAmpModelerCore) - header-only, skip full configure
CPMAddPackage(
    NAME Eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

# nlohmann_json (required by NeuralAmpModelerCore)
CPMAddPackage(
    NAME nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# --- Plugin Target ---
juce_add_plugin(OneDial
    COMPANY_NAME "OneDial"
    PLUGIN_MANUFACTURER_CODE ODia
    PLUGIN_CODE ODi1
    FORMATS AU Standalone
    PRODUCT_NAME "OneDial"
    BUNDLE_ID "com.onedial.plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    DESCRIPTION "Neural Amp Modeler with One Dial"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/onedial_icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/onedial_icon.png"
)

# --- Source Files ---
target_sources(OneDial PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/NAMEngine.cpp
    src/ZoneBlender.cpp
    src/IRLoader.cpp
)

# --- Include Directories ---
target_include_directories(OneDial PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# NeuralAmpModelerCore include paths
target_include_directories(OneDial PRIVATE
    ${NeuralAmpModelerCore_SOURCE_DIR}
    ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies/nlohmann
    ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies
)

# Eigen include path
target_include_directories(OneDial PRIVATE
    ${Eigen_SOURCE_DIR}
)

# --- Bundle Resources ---
file(GLOB NAM_PROFILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/*.nam")
file(GLOB IR_CABINETS "${CMAKE_CURRENT_SOURCE_DIR}/resources/cabinets/*.wav")

if(NAM_PROFILES OR IR_CABINETS)
    juce_add_binary_data(OneDial_Resources SOURCES ${NAM_PROFILES} ${IR_CABINETS})
    target_link_libraries(OneDial PRIVATE OneDial_Resources)
endif()

# --- JUCE Compile Definitions ---
target_compile_definitions(OneDial PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    NAM_SAMPLE_FLOAT=1
)

# --- Link Libraries ---
# NOTE: juce_recommended_lto_flags is intentionally NOT included. LTO strips
# NAM's architecture-registration static initializers (anonymous-namespace
# ConfigParserHelper objects in wavenet/model.cpp, lstm.cpp, etc.) because
# nothing external references them, which causes get_dsp() to throw
# "No config parser registered for architecture: WaveNet" at runtime.
target_link_libraries(OneDial
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(OneDial PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)

# Link NeuralAmpModelerCore.
#
# We wrap NAM's sources in a dedicated static archive and force-load it.
# Rationale: NAM registers its architectures (WaveNet, LSTM, ConvNet, Linear)
# via anonymous-namespace static initializers in each architecture's .cpp.
# When these translation units are linked directly into the plugin, Apple's
# ld runs -dead_strip and drops any .o file with no externally-referenced
# symbols — stripping the static initializers before they can register, so
# get_dsp() later throws "No config parser registered for architecture:
# WaveNet". Using a static archive + -force_load tells the linker to
# include every member of the archive, preserving the initializers.
if(TARGET nam)
    target_link_libraries(OneDial PRIVATE nam)
elseif(TARGET NeuralAmpModelerCore)
    target_link_libraries(OneDial PRIVATE NeuralAmpModelerCore)
else()
    file(GLOB_RECURSE NAM_SOURCES "${NeuralAmpModelerCore_SOURCE_DIR}/NAM/*.cpp")
    add_library(onedial_nam_core STATIC ${NAM_SOURCES})
    target_include_directories(onedial_nam_core PUBLIC
        ${NeuralAmpModelerCore_SOURCE_DIR}
        ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies/nlohmann
        ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies
        ${Eigen_SOURCE_DIR}
    )
    target_compile_features(onedial_nam_core PUBLIC cxx_std_17)
    target_compile_definitions(onedial_nam_core PUBLIC NAM_SAMPLE_FLOAT=1)
    set_target_properties(onedial_nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # INTERFACE so the -force_load option propagates to the final plugin
    # bundle (OneDial_AU, OneDial_Standalone), where the actual link happens.
    # JUCE's "OneDial" is a static aggregator; per-format targets are what ld
    # invokes, and that's where dead-stripping must be defeated.
    target_link_options(onedial_nam_core INTERFACE
        "LINKER:-force_load,$<TARGET_FILE:onedial_nam_core>")
    target_link_libraries(OneDial PRIVATE onedial_nam_core)
endif()

target_link_libraries(OneDial PRIVATE nlohmann_json::nlohmann_json)

# --- Test executable: drives ZoneBlender directly to verify the dial
#     produces distinct tonal responses across the 5 zones. Uses the same
#     NAM static archive + binary resources as the plugin.
add_executable(test_dial tools/test_dial.cpp src/ZoneBlender.cpp src/NAMEngine.cpp src/IRLoader.cpp)
target_include_directories(test_dial PRIVATE src)
target_compile_definitions(test_dial PRIVATE NAM_SAMPLE_FLOAT=1 JUCE_STANDALONE_APPLICATION=1)
set_target_properties(test_dial PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
target_link_libraries(test_dial PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    nlohmann_json::nlohmann_json
    onedial_nam_core
    OneDial_Resources
)

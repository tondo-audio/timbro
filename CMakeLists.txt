cmake_minimum_required(VERSION 3.22)

# --- Version derived from git ---
# TIMBRO_VERSION (numeric X[.Y[.Z]]) seeds project() VERSION and is read
# from the latest tag at configure time — that's fine because it only
# changes when a release is cut. The editor's display stamp is handled
# separately by cmake/GenerateVersion.cmake (see below), which runs at
# every build so a `cmake --build` after a commit reflects the new SHA
# without needing a fresh configure.
set(TIMBRO_VERSION "0.0.0")
find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_LATEST_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(GIT_LATEST_TAG)
        # CMake's regex flavor lacks {n,m} repetition, so spell out the
        # optional minor and patch components explicitly.
        string(REGEX MATCH "[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?" GIT_NUMERIC "${GIT_LATEST_TAG}")
        if(GIT_NUMERIC)
            set(TIMBRO_VERSION "${GIT_NUMERIC}")
        endif()
    endif()
endif()
message(STATUS "Timbro project version: ${TIMBRO_VERSION}")

project(Timbro VERSION ${TIMBRO_VERSION} LANGUAGES C CXX)

# C++20 is required because NAM's slimmable wavenet uses
# std::atomic<std::shared_ptr<T>>, a C++20 addition. libc++/libstdc++
# tolerate it as an extension under -std=c++17, but MSVC's STL strictly
# rejects it via static_assert, breaking the Windows build.
set(CMAKE_CXX_STANDARD 20)
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
# EXCLUDE_FROM_ALL keeps NAM's auxiliary tools (benchmodel, render) out
# of the default build. They use GCC-only flags like -Wno-error that
# MSVC rejects with D8021, and we don't link against them anyway —
# we glob NAM/*.cpp ourselves into timbro_nam_core below.
CPMAddPackage(
    NAME NeuralAmpModelerCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG main
    EXCLUDE_FROM_ALL YES
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
# Plugin formats: AU only on Apple, VST3+Standalone everywhere.
if(APPLE)
    set(TIMBRO_FORMATS AU VST3 Standalone)
else()
    set(TIMBRO_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Timbro
    COMPANY_NAME "Tondo Audio"
    PLUGIN_MANUFACTURER_CODE TmbR
    PLUGIN_CODE Tmb1
    FORMATS ${TIMBRO_FORMATS}
    PRODUCT_NAME "Timbro"
    BUNDLE_ID "audio.tondo.timbro"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Timbro needs audio input access to process your guitar signal."
    DESCRIPTION "Neural Amp Modeler with One Dial"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/timbro_icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/timbro_icon.png"
)

# --- Source Files ---
target_sources(Timbro PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/NAMEngine.cpp
    src/ZoneBlender.cpp
    src/IRLoader.cpp
)

# --- Include Directories ---
target_include_directories(Timbro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# NeuralAmpModelerCore include paths
target_include_directories(Timbro PRIVATE
    ${NeuralAmpModelerCore_SOURCE_DIR}
    ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies/nlohmann
    ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies
)

# Eigen include path
target_include_directories(Timbro PRIVATE
    ${Eigen_SOURCE_DIR}
)

# --- Bundle Resources ---
file(GLOB NAM_PROFILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/*.nam")
file(GLOB IR_CABINETS "${CMAKE_CURRENT_SOURCE_DIR}/resources/cabinets/*.wav")
file(GLOB UI_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/resources/ui/*.png")
file(GLOB UI_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/*.ttf")

if(NAM_PROFILES OR IR_CABINETS OR UI_ASSETS OR UI_FONTS)
    juce_add_binary_data(Timbro_Resources SOURCES
        ${NAM_PROFILES} ${IR_CABINETS} ${UI_ASSETS} ${UI_FONTS})
    target_link_libraries(Timbro PRIVATE Timbro_Resources)
endif()

# --- JUCE Compile Definitions ---
target_compile_definitions(Timbro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    NAM_SAMPLE_FLOAT=1
)

# --- Generated version header (regenerated at every build) ---
# The editor's bottom-right stamp pulls TIMBRO_DISPLAY_VERSION from this
# header. Running git describe via a custom_target keeps the stamp in
# sync with the working-tree state at every `cmake --build`, instead of
# being frozen at the last `cmake -B build`.
set(TIMBRO_VERSION_HEADER ${CMAKE_BINARY_DIR}/timbro_version.h)
add_custom_target(timbro_version ALL
    BYPRODUCTS ${TIMBRO_VERSION_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT_FILE=${TIMBRO_VERSION_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake
    COMMENT "Refreshing Timbro version header from git")
add_dependencies(Timbro timbro_version)
target_include_directories(Timbro PRIVATE ${CMAKE_BINARY_DIR})

# --- Link Libraries ---
# NOTE: juce_recommended_lto_flags is intentionally NOT included. The real
# fix that keeps NAM's architecture registrators alive is the -force_load
# below, but LTO would still aggressively prune the same anonymous-namespace
# ConfigParserHelper objects in wavenet/model.cpp, lstm.cpp etc. Skipping
# the LTO recommendation removes that risk entirely.
target_link_libraries(Timbro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# JUCE 8 on Linux still emits libcurl symbol references from juce_core.cpp
# even when JUCE_USE_CURL=0 is defined on the plugin target — the macro
# does not propagate cleanly into the JUCE module's translation unit
# under the current module structure. Linking libcurl resolves the
# undefined references; on macOS (CFNetwork) and Windows (WinINet) JUCE
# does not touch curl, so this stays Linux-only.
if(UNIX AND NOT APPLE)
    find_package(CURL REQUIRED)
    # PUBLIC so the dependency reaches the per-format link targets
    # (Timbro_VST3, Timbro_Standalone) — juce_core.cpp's curl references
    # live in their object files via JUCE's INTERFACE_SOURCES propagation,
    # so curl must be on their link line, not just the shared-code archive.
    target_link_libraries(Timbro PUBLIC CURL::libcurl)
endif()

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
    target_link_libraries(Timbro PRIVATE nam)
elseif(TARGET NeuralAmpModelerCore)
    target_link_libraries(Timbro PRIVATE NeuralAmpModelerCore)
else()
    file(GLOB_RECURSE NAM_SOURCES "${NeuralAmpModelerCore_SOURCE_DIR}/NAM/*.cpp")
    add_library(timbro_nam_core STATIC ${NAM_SOURCES})
    target_include_directories(timbro_nam_core PUBLIC
        ${NeuralAmpModelerCore_SOURCE_DIR}
        ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies/nlohmann
        ${NeuralAmpModelerCore_SOURCE_DIR}/Dependencies
        ${Eigen_SOURCE_DIR}
    )
    target_compile_features(timbro_nam_core PUBLIC cxx_std_20)
    target_compile_definitions(timbro_nam_core PUBLIC NAM_SAMPLE_FLOAT=1)
    set_target_properties(timbro_nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # INTERFACE so the option propagates to the final plugin bundle
    # (Timbro_AU, Timbro_Standalone) where ld actually runs. JUCE's
    # "Timbro" is a static aggregator; per-format targets are what link.
    # macOS uses Apple ld's -force_load. GNU ld uses --whole-archive.
    if(APPLE)
        target_link_options(timbro_nam_core INTERFACE
            "LINKER:-force_load,$<TARGET_FILE:timbro_nam_core>")
    elseif(MSVC)
        # MSVC: /WHOLEARCHIVE:<libname> preserves all .obj members of a
        # static archive, mirroring -force_load on Apple ld and
        # --whole-archive on GNU ld. Reference by target name; the linker
        # resolves it from the library search paths CMake already passes.
        target_link_options(timbro_nam_core INTERFACE
            "LINKER:/WHOLEARCHIVE:timbro_nam_core")
    elseif(UNIX)
        target_link_options(timbro_nam_core INTERFACE
            "LINKER:--whole-archive,$<TARGET_FILE:timbro_nam_core>,--no-whole-archive")
    else()
        message(FATAL_ERROR "Unsupported platform for NAM force-load: "
                "no equivalent of -force_load / /WHOLEARCHIVE configured.")
    endif()
    target_link_libraries(Timbro PRIVATE timbro_nam_core)
endif()

target_link_libraries(Timbro PRIVATE nlohmann_json::nlohmann_json)

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
    timbro_nam_core
    Timbro_Resources
)

# test_dial pulls in juce_core.cpp directly, so the same Linux libcurl
# link applies here (see comment on the plugin target above).
if(UNIX AND NOT APPLE)
    target_link_libraries(test_dial PRIVATE CURL::libcurl)
endif()

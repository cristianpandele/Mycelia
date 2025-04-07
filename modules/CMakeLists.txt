add_subdirectory(JUCE)
include(${CMAKE_SOURCE_DIR}/cmake/SubprojectVersion.cmake)
subproject_version(JUCE juce_version)
message(STATUS "VERSION for JUCE: ${juce_version}")

include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

if (NOT TARGET fmt)
    CPMAddPackage(NAME fmt
            GITHUB_REPOSITORY fmtlib/fmt
            GIT_TAG master
            )
endif ()

if (NOT TARGET sst-cpputils)
    CPMAddPackage(NAME sst-cpputils
            GITHUB_REPOSITORY surge-synthesizer/sst-cpputils
            GIT_TAG main
    )
endif()

# Add SIMDE for cross-platform vector instructions
include(FetchContent)
FetchContent_Declare(
    simde
    GIT_REPOSITORY https://github.com/simd-everywhere/simde.git
    GIT_TAG master
)
FetchContent_MakeAvailable(simde)

if (NOT TARGET simde)
    message(STATUS "Importing SIMDE with CPM")
    CPMAddPackage(NAME simde
            GITHUB_REPOSITORY simd-everywhere/simde
            VERSION 0.7.2
            )
endif ()

if (NOT TARGET eurorack)
    CPMAddPackage(NAME eurorack
            GITHUB_REPOSITORY surge-synthesizer/eurorack
            GIT_TAG surge
    )
endif ()

# Add CLAP format
add_subdirectory(clap-juce-extensions EXCLUDE_FROM_ALL)

# Add polylogarithm module
add_subdirectory(polylogarithm)

# Add Surge modules and dependencies
# Create a linkable target for SIMDE
add_library(simde INTERFACE)
target_include_directories(simde INTERFACE ${simde_SOURCE_DIR})

# Set up the eurorack library
add_library(eurorack INTERFACE)
target_include_directories(eurorack INTERFACE ${eurorack_SOURCE_DIR})

# Define surge library
add_library(surge INTERFACE)
add_code_compile_defaults(surge)

# Add Surge effects
add_subdirectory(sst/sst-basic-blocks)
add_subdirectory(sst/sst-effects)
add_subdirectory(sst/sst-filters)
add_subdirectory(sst/sst-waveshapers)
target_link_libraries(surge INTERFACE sst-basic-blocks simde fmt sst-effects eurorack sst-filters sst-waveshapers)
target_compile_definitions(surge INTERFACE SST_EFFECTS_EURORACK=1)

# Add any extra JUCE modules
juce_add_modules(
    foleys_gui_magic/modules/foleys_gui_magic
)

# This lets us use our code in both the JUCE targets and our Test target
# Without running into ODR violations
add_library(ModulesCode INTERFACE)

# Enable fast math, C++20 and a few other target defaults
add_code_compile_defaults(ModulesCode)

target_compile_definitions(
    ModulesCode
    INTERFACE
    FOLEYS_SHOW_GUI_EDITOR_PALLETTE=${SHOW_GUI_EDITOR}
)

# Link to any other modules you added (with juce_add_module) here!
# Usually JUCE modules must have PRIVATE visibility
# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md#juce_add_module
# However, with Pamplejuce, you'll link modules to ModulesCode with INTERFACE visibility
# This allows the JUCE plugin targets and the Tests target to link against it
target_link_libraries(
    ModulesCode
    INTERFACE
        # Assets
        BinaryData
        # Juce
        juce_audio_utils
        # juce::juce_audio_plugin_client
        juce_audio_processors
        juce_dsp
        juce_core
        juce_gui_basics
        juce_gui_extra
        # Foleys GUI Magic
        foleys_gui_magic
        # Polylogarithm
        polylogarithm
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        # Link to surge which includes sst libraries
        surge
)

# Below from ChowMatrix
include(cmake/pluginval.cmake)

# include(cmake/WarningFlags.cmake)
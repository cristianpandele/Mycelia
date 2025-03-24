add_subdirectory(JUCE)
include(${CMAKE_SOURCE_DIR}/cmake/SubprojectVersion.cmake)
subproject_version(JUCE juce_version)
message(STATUS "VERSION for JUCE: ${juce_version}")

# Add CLAP format
add_subdirectory(clap-juce-extensions EXCLUDE_FROM_ALL)

# Add polylogarithm module
add_subdirectory(polylogarithm)

# Add any extra JUCE modules
juce_add_modules(
    foleys_gui_magic/modules/foleys_gui_magic
)

# This lets us use our code in both the JUCE targets and our Test target
# Without running into ODR violations
add_library(ModulesCode INTERFACE)

# Enable fast math, C++20 and a few other target defaults
add_code_compile_defaults(ModulesCode)

# Link to any other modules you added (with juce_add_module) here!
# Usually JUCE modules must have PRIVATE visibility
# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md#juce_add_module
# However, with Pamplejuce, you'll link modules to ModulesCode with INTERFACE visibility
# This allows the JUCE plugin targets and the Tests target to link against it
target_link_libraries(ModulesCode
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
)

# Below from ChowMatrix
include(cmake/pluginval.cmake)

# include(cmake/WarningFlags.cmake)
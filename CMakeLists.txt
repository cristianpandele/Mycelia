cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.3" CACHE STRING "Minimum OS X deployment target")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)

# Modern concise way to add dependencies to your project
include (CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# Enable fast math, C++20 and a few other target defaults
include(LibCodeDefaults)

# This is the internal name of the project and the name of JUCE's shared code target
# Note: This cannot have spaces (it may be 2024, but you can't have it all!)
# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs display)
set(PROJECT_NAME "Mycelia")

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)
# You can also just have it be the same thing as PROJECT_NAME
# You may want to append the major version on the end of this (and PROJECT_NAME) ala:
#   set(PROJECT_NAME "MyPlugin_v${MAJOR_VERSION}")
# Doing so enables major versions to show up in IDEs and DAWs as separate plugins
# allowing you to change parameters and behavior without breaking existing user projects
set(PRODUCT_NAME "Mycelial Delay")

# Used for the MacOS bundle name and Installers
set(COMPANY_NAME "AetherEar")

# Used for the MacOS bundle identifier (and signing)
set(BUNDLE_ID "com.aetherear.mycelia")

# Set the plugin formats you want built
# Valid choices: AAX Unity VST VST3 AU AUv3 Standalone
set(FORMATS Standalone AU VST3 AUv3)

# For simplicity, the name of the CMake project is also the name of the target
project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

add_subdirectory(modules)
include_directories(modules)

# juce_set_vst2_sdk_path(C:/SDKs/VST_SDK/VST2_SDK/)
# juce_set_aax_sdk_path(NONE)

# set default plugin formats to build
if(IOS)
    set(JUCE_FORMATS Standalone AUv3)
else()
    set(JUCE_FORMATS AU VST3 Standalone)
endif()

# Build LV2 only on Linux
if(UNIX AND NOT APPLE)
    message(STATUS "Building LV2 plugin format")
    list(APPEND JUCE_FORMATS LV2)
endif()

# Build VST2 if SDK target exists
if(TARGET juce_vst2_sdk)
    message(STATUS "Building VST2 plugin format")
    list(APPEND JUCE_FORMATS VST)
endif()

# Build AAX if SDK target exists
if(TARGET juce_aax_sdk)
    message(STATUS "Building AAX plugin format")
    list(APPEND JUCE_FORMATS AAX)
endif()

# See `docs/CMake API.md` in the JUCE repo for all config options
juce_add_plugin(
    "${PROJECT_NAME}"

    # Icons for the standalone app
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.png"

    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"

    # On MacOS, plugin is copied to /Users/yourname/Library/Audio/Plug-Ins/
    COPY_PLUGIN_AFTER_BUILD TRUE

    # A four-character plugin id
    # First character MUST be uppercase for AU formats
    PLUGIN_MANUFACTURER_CODE "Aeth"

    # A unique four-character plugin id
    # Note: this must have at least one upper-case character
    PLUGIN_CODE "mycD"
    FORMATS "${JUCE_FORMATS}"

    # The name of your final executable
    # This is how it's listed in the DAW
    # This can be different from PROJECT_NAME and can have spaces!
    # You might want to use v${MAJOR_VERSION} here once you go to v2...
    PRODUCT_NAME "${PRODUCT_NAME}"

    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_Delay

    LV2URI https://github.com/cristianpandele/Mycelia

    ICON_BIG res/logo.png
    MICROPHONE_PERMISSION_ENABLED TRUE
    NEEDS_STORE_KIT TRUE
    REQUIRES_FULL_SCREEN TRUE
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
)

# add sources
add_subdirectory(src)
include_directories(src)
# add resources
add_subdirectory(res)

# This is where you can set preprocessor definitions for JUCE and your plugin
target_compile_definitions("${PROJECT_NAME}"
    PUBLIC

    # JUCE_WEB_BROWSER and JUCE_USE_CURL off by default
    JUCE_WEB_BROWSER=0  # If you set this to 1, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0     # If you set this to 1, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_VST3_CAN_REPLACE_VST2=0

    # Uncomment if you are paying for a an Indie/Pro license or releasing under GPLv3
    # JUCE_DISPLAY_SPLASH_SCREEN=0

    # lets the app known if we're Debug or Release
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"

    # JucePlugin_Name is for some reason doesn't use the nicer PRODUCT_NAME
    PRODUCT_NAME_WITHOUT_VERSION="Mycelia"
)

add_code_compile_defaults(${PROJECT_NAME})

# Link the JUCE plugin targets our AppCode target
target_link_libraries("${PROJECT_NAME}" PRIVATE AppCode)

# IPP support, comment out to disable
# include(PamplejuceIPP)

# Everything related to the tests target
include(Tests)

# A separate target for Benchmarks (keeps the Tests target fast)
include(Benchmarks)

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)

# Below from ChowMatrix
# # ASIO
# if(ASIOSDK_DIR)
#     message(STATUS "Using ASIO SDK from ${ASIOSDK_DIR}")
#     target_include_directories(juce_plugin_modules PUBLIC ${ASIOSDK_DIR}/common)
#     target_compile_definitions(juce_plugin_modules PUBLIC JUCE_ASIO=1)
# endif()

# Pluginval
option(RUN_PLUGINVAL "Run pluginval on Mycelia plugins" OFF)
if(RUN_PLUGINVAL)
    create_pluginval_target("${PROJECT_NAME}"_VST3 "Mycelia.vst3")
endif()

# # we need these flags for notarization on MacOS
# option(MACOS_RELEASE "Set build flags for MacOS Release" OFF)
# if(MACOS_RELEASE)
#     message(STATUS "Setting MacOS release flags...")
#     set_target_properties("${PROJECT_NAME}"_Standalone PROPERTIES
#         XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES)
# endif()

# # iOS
# if(IOS)
#     message(STATUS "Setting iOS-specific properties...")

#     foreach(target IN ITEMS BinaryData polylogarithm juce_plugin_modules "${PROJECT_NAME}" "${PROJECT_NAME}"_Standalone "${PROJECT_NAME}"_AUv3)
#         set_target_properties(${target}
#             PROPERTIES
#                 RUNTIME_OUTPUT_DIRECTORY "./"
#                 ARCHIVE_OUTPUT_DIRECTORY "./"
#                 LIBRARY_OUTPUT_DIRECTORY "./")
#     endforeach()

#     set_target_properties(Mycelia_Standalone PROPERTIES
#             XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
#             XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
#             XCODE_ATTRIBUTE_ENABLE_IN_APP_PURCHASE "YES")

#     set_target_properties(Mycelia_AUv3 PROPERTIES
#             XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)/Mycelia.app/PlugIns"
#             XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
#             XCODE_ATTRIBUTE_ENABLE_IN_APP_PURCHASE "YES")
# endif()

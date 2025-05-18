# This lets us use our code in both the JUCE targets and our Test target
# Without running into ODR violations
add_library(AppCode INTERFACE)

# Enable fast math, C++20 and a few other target defaults
add_code_compile_defaults(AppCode)

target_sources(
    AppCode
    INTERFACE
    Mycelia.cpp
    MyceliaModel.cpp
    MyceliaView.cpp
    dsp/InputNode.cpp
    dsp/EdgeTree.cpp
    dsp/EnvelopeFollower.cpp
    dsp/DelayNetwork.cpp
    dsp/Sky.cpp
    dsp/DelayNodes.cpp
    dsp/DelayProc.cpp
    dsp/DiffusionControl.cpp
    dsp/Dispersion.cpp
    dsp/DuckingCompressor.cpp
    dsp/OutputNode.cpp
    gui/DuckLevelAnimation.cpp
    gui/FoldWindowAnimation.cpp
    gui/NetworkGraphAnimation.cpp
    gui/TreePositionAnimation.cpp
)

# Link the actual app code to the ModulesCode library
target_link_libraries(
    AppCode
    INTERFACE
    ModulesCode
)


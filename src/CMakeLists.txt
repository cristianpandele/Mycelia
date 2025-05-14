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

    # dsp/BaseNode.cpp
    # dsp/DelayNode.cpp
    # # dsp/Delay/DelayProc.cpp
    # # dsp/Delay/Diffusion.cpp
    # # dsp/Delay/Reverser.cpp
    # # dsp/Delay/PitchShiftWrapper.cpp
    # dsp/Delay/VariableDelay.cpp
    # # dsp/Distortion/Distortion.cpp
    # # dsp/Distortion/LookupTables.cpp
    # # dsp/Parameters/BaseController.cpp
    # # dsp/Parameters/DelayTypeControl.cpp
    # dsp/Parameters/HostParamControl.cpp
    # # dsp/Parameters/InsanityControl.cpp
    # # dsp/Parameters/InsanityLockHelper.cpp
    # # dsp/Parameters/NodeParamControl.cpp
    # # dsp/Parameters/RandomiseLockHelper.cpp
    # # dsp/Parameters/ParamHelpers.cpp
    # # dsp/Parameters/SyncControl.cpp

    # gui/AutoUpdating.cpp
    # gui/ParamSlider.cpp
    # gui/BottomBar/ABComp.cpp
    # gui/BottomBar/BottomBarLNF.cpp
    # gui/BottomBar/HostControlMenu.cpp
    # gui/BottomBar/HostControlMenuComp.cpp
    # gui/DetailsView/NodeDetails.cpp
    # gui/DetailsView/NodeDetailsComponent.cpp
    # gui/DetailsView/NodeDetailsGUI.cpp
    # gui/DetailsView/NodeDetailsViewport.cpp
    # gui/IOSUtils/LongPressActionHelper.cpp
    # gui/IOSUtils/TipJar.cpp
    # gui/MatrixView/DelayNodeComponent.cpp
    # gui/MatrixView/GraphView.cpp
    # gui/MatrixView/GraphViewport.cpp
    # gui/MatrixView/MatrixAurora.cpp
    # gui/MatrixView/NodeCompManager.cpp
    # gui/MatrixView/NodeComponent.cpp

    # state/PresetManager.cpp
    # state/StateManager.cpp
)

# Link the actual app code to the ModulesCode library
target_link_libraries(
    AppCode
    INTERFACE
    ModulesCode
)


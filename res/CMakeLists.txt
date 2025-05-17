juce_add_binary_data(BinaryData SOURCES
    sporadic.xml
    duckSmallBackgroundRemoved.png
    tree_1.png
    tree_2.png
    tree_3.png
)

# Need to build BinaryData with -fPIC flag on Linux
set_target_properties(BinaryData PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

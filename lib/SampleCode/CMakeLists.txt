project(SampleCode)
include(${SDK_LINUX_DIR}/CMake/project.cmake)
add_library(SampleCode STATIC
    SampleCode.h
    Gui.h Gui.cpp
    Log.h Log.cpp
)
target_link_libraries(SampleCode
    PUBLIC ImGui StructureLinux
    PRIVATE GLEW glfw
)

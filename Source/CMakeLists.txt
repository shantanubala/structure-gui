project(Playground)
include(${SDK_LINUX_DIR}/CMake/project.cmake)
add_executable(Playground
    Gui.h Gui.cpp
    Playground.h Playground.cpp
)
target_link_libraries(Playground PRIVATE
    StructureLinux
    GLEW
    SampleCode
    pthread # Needed for std::condition_variable to work properly
)
copy_libs_alongside_executable(Playground StructureLinux)
set_executable_rpath_adjacent(Playground)
use_link_time_rpath_for(Playground)

project(Recorder)
include(${SDK_LINUX_DIR}/CMake/project.cmake)
add_executable(Recorder
    Gui.h Gui.cpp
    Recorder.h Recorder.cpp
)
target_link_libraries(Recorder PRIVATE
    StructureLinux
    GLEW
    SampleCode
    pthread # Needed for std::condition_variable to work properly
)
copy_libs_alongside_executable(Recorder StructureLinux)
set_executable_rpath_adjacent(Recorder)
use_link_time_rpath_for(Recorder)

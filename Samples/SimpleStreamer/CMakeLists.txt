project(SimpleStreamer)
include(${SDK_LINUX_DIR}/CMake/project.cmake)
add_executable(SimpleStreamer
    SimpleStreamer.cpp
)
target_link_libraries(SimpleStreamer PRIVATE
    StructureLinux
)
copy_libs_alongside_executable(SimpleStreamer StructureLinux)
set_executable_rpath_adjacent(SimpleStreamer)
use_link_time_rpath_for(SimpleStreamer)

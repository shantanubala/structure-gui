cmake_minimum_required(VERSION 3.8.0)
project(StructureLinux)
set(SDK_LINUX_DIR ${PROJECT_SOURCE_DIR})
if(NOT LINUX_TARGET_ARCH)
    message(FATAL_ERROR "LINUX_TARGET_ARCH required but not set (build.sh will do this automatically)")
endif()
add_library(StructureLinux SHARED IMPORTED)
set_target_properties(StructureLinux PROPERTIES
    IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/Libraries/Structure/${LINUX_TARGET_ARCH}/lib/libStructureLinux.so
    INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/Libraries/Structure/${LINUX_TARGET_ARCH}/include
)
include(CMake/extlibs.cmake)
add_subdirectory(Libraries/SampleCode)
add_subdirectory(External/ImGui)
add_subdirectory(Source)
add_subdirectory(Samples/Recorder)
add_subdirectory(Samples/SimpleStreamer)
add_custom_target(Samples DEPENDS Recorder SimpleStreamer)

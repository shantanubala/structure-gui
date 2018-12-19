# Include this file on a per-project basis. It should not be included globally
# because these settings should not propagate to externally linked projects, if
# any.

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
else()
    message(FATAL_ERROR "Don't know how to enable warnings for compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

include_directories(${SDK_LINUX_DIR}/External)
include_directories(${SDK_LINUX_DIR}/Libraries)

# Sometimes needed for cross/sysroot builds.
function(use_link_time_rpath_for target)
    if(LINUX_LINK_TIME_RPATH)
        string(REPLACE ";" ":" paths "${LINUX_LINK_TIME_RPATH}")
        set(linkRpathFlags "-Wl,-rpath-link,${paths}")
        get_target_property(linkFlags ${target} LINK_FLAGS)
        if(linkFlags)
            set(linkFlags "${linkFlags} ${linkRpathFlags}")
        else()
            set(linkFlags ${linkRpathFlags})
        endif()
        set_target_properties(${target} PROPERTIES
            LINK_FLAGS "${linkFlags}"
        )
    endif()
endfunction()

# Place a copy of the library next to the executable and link against it.
function(copy_libs_alongside_executable executable)
    foreach(lib ${ARGN})
        if(NOT TARGET ${lib})
            message(FATAL_ERROR "${lib} is not a target, don't know how to copy it")
        endif()
        set(libDestPath ${CMAKE_CURRENT_BINARY_DIR}/lib${lib}.so)
        set(genTargetName copylib_${lib}_for_${executable})
        add_custom_command(OUTPUT ${libDestPath}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${lib}> ${libDestPath}
            DEPENDS ${lib}
        )
        add_custom_target(${genTargetName} DEPENDS ${libDestPath})
        add_dependencies(${executable} ${genTargetName})
    endforeach()
endfunction()

# Set the executable's embedded rpath to allow loading libraries in the same
# directory and obviate the need for LD_LIBRARY_PATH shenanigans.
function(set_executable_rpath_adjacent executable)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # $ORIGIN is interpreted at load time
        set(rpathFlags -Wl,-rpath,$ORIGIN)
    else()
        message(FATAL_ERROR "Don't know how to set library rpath for: ${CMAKE_CXX_COMPILER_ID}")
    endif()
    get_target_property(linkFlags ${executable} LINK_FLAGS)
    if(linkFlags)
        set(linkFlags "${linkFlags} ${rpathFlags}")
    else()
        set(linkFlags ${rpathFlags})
    endif()
    set_target_properties(${executable} PROPERTIES
        SKIP_BUILD_RPATH YES
        LINK_FLAGS "${linkFlags}"
    )
endfunction()

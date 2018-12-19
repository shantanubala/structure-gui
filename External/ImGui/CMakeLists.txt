project(ImGui)
include(${SDK_LINUX_DIR}/CMake/project.cmake)

find_library(GL_PATH GL)
if(NOT GL_PATH)
    message(FATAL_ERROR "GL not found")
endif()

find_library(EGL_PATH EGL)
if(NOT EGL_PATH)
    message(FATAL_ERROR "EGL not found")
endif()

add_compile_options(-Wno-class-memaccess)
add_library(ImGui STATIC
    imconfig.h
    imgui.h
    imgui_impl_glfw.h
    imgui_impl_opengl3.h
    imgui_internal.h
    stb_rect_pack.h
    stb_textedit.h
    stb_truetype.h
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_impl_glfw.cpp
    imgui_impl_opengl3.cpp
)
target_link_libraries(ImGui PRIVATE ${GL_PATH} ${EGL_PATH} GLEW glfw)

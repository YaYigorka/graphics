cmake_minimum_required(VERSION 3.20)
project(Dodecahedron LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

foreach(SHADER vertex.vert fragment.frag)
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_SPV "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT "${SHADER_SPV}"
        COMMAND "${GLSLC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}" -o "${SHADER_SPV}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}"
        VERBATIM
    )
    list(APPEND SHADER_BINARIES "${SHADER_SPV}")
endforeach()
add_custom_target(shaders ALL DEPENDS ${SHADER_BINARIES})

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC Vulkan::Vulkan glfw)

add_executable(dodecahedron
    src/main.cpp
    src/math.cpp
    src/geometry.cpp
)
add_dependencies(dodecahedron shaders)
target_include_directories(dodecahedron PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(dodecahedron PRIVATE Vulkan::Vulkan glfw imgui_lib)
target_compile_definitions(dodecahedron PRIVATE
    SHADER_DIR="${SHADER_OUTPUT_DIR}"
)

if(APPLE)
    target_compile_options(dodecahedron PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

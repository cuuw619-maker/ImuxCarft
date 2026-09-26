cmake_minimum_required(VERSION 3.24)
project(ImuxCarft VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW CONFIG REQUIRED)

file(GLOB_RECURSE IMUX_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/src/*.cpp"
)

add_executable(ImuxCarft ${IMUX_ENGINE_SOURCES})
target_include_directories(ImuxCarft PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/engine/include")
target_link_libraries(ImuxCarft PRIVATE SDL2::SDL2 SDL2_image::SDL2_image OpenGL::GL GLEW::GLEW)

if(WIN32)
    target_link_libraries(ImuxCarft PRIVATE opengl32)
endif()

if(MSVC)
    target_compile_options(ImuxCarft PRIVATE /W4)
else()
    target_compile_options(ImuxCarft PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET ImuxCarft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ImuxCarft>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ImuxCarft>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ImuxCarft>/background"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/background" "$<TARGET_FILE_DIR:ImuxCarft>/background"
)

message(STATUS "ImuxCarft assets: ${CMAKE_CURRENT_SOURCE_DIR}/assets")
message(STATUS "ImuxCarft backgrounds: ${CMAKE_CURRENT_SOURCE_DIR}/background")

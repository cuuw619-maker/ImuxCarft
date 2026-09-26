cmake_minimum_required(VERSION 3.24)
project(ImuxCarft VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(IMUXCARFT_BUILD_CSHARP "Build the C# module when dotnet is available" OFF)
option(IMUXCARFT_BUILD_RUST "Build the Rust module when cargo is available" OFF)

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()
find_package(OpenGL REQUIRED)

file(GLOB_RECURSE IMUX_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/src/*.cpp"
)

add_executable(ImuxCarft ${IMUX_ENGINE_SOURCES})
target_include_directories(ImuxCarft PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/engine/include")

if(TARGET SDL2::SDL2)
    target_link_libraries(ImuxCarft PRIVATE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(ImuxCarft PRIVATE SDL2::SDL2-static)
else()
    target_include_directories(ImuxCarft PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(ImuxCarft PRIVATE ${SDL2_LIBRARIES})
endif()
target_link_libraries(ImuxCarft PRIVATE OpenGL::GL)
if(WIN32)
    target_link_libraries(ImuxCarft PRIVATE opengl32)
endif()
if(MSVC)
    target_compile_options(ImuxCarft PRIVATE /W4)
else()
    target_compile_options(ImuxCarft PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET ImuxCarft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:ImuxCarft>/assets"
)

if(IMUXCARFT_BUILD_CSHARP)
    find_program(DOTNET_EXECUTABLE dotnet)
    if(DOTNET_EXECUTABLE)
        add_custom_target(ImuxCarftCSharp ALL
            COMMAND ${DOTNET_EXECUTABLE} build
                    "${CMAKE_CURRENT_SOURCE_DIR}/game-csharp/ImuxCarft.Game.csproj"
                    --configuration $<CONFIG>
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/game-csharp"
        )
    endif()
endif()

if(IMUXCARFT_BUILD_RUST)
    find_program(CARGO_EXECUTABLE cargo)
    if(CARGO_EXECUTABLE)
        add_custom_target(ImuxCarftRust ALL
            COMMAND ${CARGO_EXECUTABLE} build --release
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/systems-rust"
        )
    endif()
endif()

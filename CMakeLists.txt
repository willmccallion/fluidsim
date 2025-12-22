cmake_minimum_required(VERSION 3.14)
project(FluidSimC)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Raylib Setup ---
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG master
)
FetchContent_MakeAvailable(raylib)

# --- Sources ---
set(SOURCES
    src/main.c
    src/fluid.c
    src/renderer.c
)

# --- Executable ---
add_executable(fluid_sim ${SOURCES})

# --- Include Directories ---
target_include_directories(fluid_sim PRIVATE ${CMAKE_SOURCE_DIR}/include)

# --- Link Libraries ---
if(UNIX AND NOT APPLE)
    target_link_libraries(fluid_sim PRIVATE raylib m)
else()
    target_link_libraries(fluid_sim PRIVATE raylib)
endif()

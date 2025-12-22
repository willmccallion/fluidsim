cmake_minimum_required(VERSION 3.14)
project(FluidSimC)

set(CMAKE_C_STANDARD 99)

# --- CRITICAL: FORCE OPENGL 4.3 ---
# This fixes context creation errors on NVIDIA
set(GRAPHICS GRAPHICS_API_OPENGL_43 CACHE STRING "Force GL 4.3" FORCE)

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
    src/simulation.c
    src/renderer.c
    src/utils.c
)

# --- Resources ---
file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})

# --- Executable ---
add_executable(fluid_sim ${SOURCES})

# --- Include Directories ---
target_include_directories(fluid_sim PRIVATE ${CMAKE_SOURCE_DIR}/include)

# --- Link Libraries ---
if(UNIX AND NOT APPLE)
    target_link_libraries(fluid_sim PRIVATE raylib m GL)
else()
    target_link_libraries(fluid_sim PRIVATE raylib)
endif()

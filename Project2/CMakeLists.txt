cmake_minimum_required(VERSION 3.10)
project(Project2Renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All include paths are exposed flat so the migrated Task-2 headers
# (which use unqualified #include "Foo.hpp") keep working after the
# subdirectory reorg.
set(PROJECT2_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/geometry
    ${CMAKE_CURRENT_SOURCE_DIR}/src/material
    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/light
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
)

file(GLOB_RECURSE PROJECT2_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.hpp
)

if(APPLE)
    foreach(LIBOMP_PREFIX "/opt/homebrew/opt/libomp" "/usr/local/opt/libomp")
        if(EXISTS "${LIBOMP_PREFIX}/include" AND EXISTS "${LIBOMP_PREFIX}/lib/libomp.dylib")
            set(OpenMP_C_FLAGS   "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include" CACHE STRING "OpenMP C flags"   FORCE)
            set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include" CACHE STRING "OpenMP CXX flags" FORCE)
            set(OpenMP_C_LIB_NAMES   "omp" CACHE STRING "OpenMP C libs"   FORCE)
            set(OpenMP_CXX_LIB_NAMES "omp" CACHE STRING "OpenMP CXX libs" FORCE)
            set(OpenMP_omp_LIBRARY "${LIBOMP_PREFIX}/lib/libomp.dylib"     CACHE FILEPATH "OpenMP libomp library" FORCE)
            list(APPEND CMAKE_PREFIX_PATH "${LIBOMP_PREFIX}")
            break()
        endif()
    endforeach()
endif()

find_package(OpenMP)

add_executable(Project2Renderer ${PROJECT2_SOURCES})
target_include_directories(Project2Renderer PRIVATE ${PROJECT2_INCLUDE_DIRS})

if(OpenMP_CXX_FOUND)
    target_link_libraries(Project2Renderer PUBLIC OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP found. Compiling with OpenMP support.")
else()
    message(STATUS "OpenMP not found. Compiling without OpenMP support.")
endif()

# std::filesystem on some older toolchains needs an explicit link.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(Project2Renderer PRIVATE stdc++fs)
endif()

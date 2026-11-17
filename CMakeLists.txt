cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(rig VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v3.1.0
)

set(TINYGLTF3_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINYGLTF3_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

configure_file(
    "${tinygltf_SOURCE_DIR}/tiny_gltf_v3.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/tiny_gltf_v3.h"
    COPYONLY
)

add_library(tinygltf3 STATIC "${tinygltf_SOURCE_DIR}/tiny_gltf_v3.c")
set_target_properties(tinygltf3 PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_include_directories(tinygltf3 PUBLIC "${tinygltf_SOURCE_DIR}")
target_compile_definitions(tinygltf3 PRIVATE TINYGLTF3_ENABLE_FS)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC "${tinygltf_SOURCE_DIR}")
target_link_libraries(${PROJECT_NAME} tinygltf3)

if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
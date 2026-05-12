cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(rig VERSION 1.0.0)

include(FetchContent)

FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG release
)

FetchContent_MakeAvailable(tinygltf)

# --- NEW VENDORING LOGIC ---
# Define where we want to copy the headers inside your include folder
set(EXT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/rig/ext")

# Create the directory if it doesn't exist
file(MAKE_DIRECTORY ${EXT_INCLUDE_DIR})

# Copy the headers from the fetched tinygltf repo directly into your mod's include folder
file(COPY 
    "${tinygltf_SOURCE_DIR}/tiny_gltf.h"
    "${tinygltf_SOURCE_DIR}/stb_image.h"
    "${tinygltf_SOURCE_DIR}/stb_image_write.h"
    "${tinygltf_SOURCE_DIR}/json.hpp"
    DESTINATION ${EXT_INCLUDE_DIR}
)
# ---------------------------

file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# We no longer need to link tinygltf_SOURCE_DIR as PRIVATE because 
# the headers are now physically inside your own include directory!

if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

# This include_directories call will now pick up the copied tiny_gltf headers
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/rig/ext")
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
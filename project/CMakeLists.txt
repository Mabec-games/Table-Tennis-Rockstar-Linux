# tabletennis - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(tabletennis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/rexglue.cmake)

# Sources
set(TABLETENNIS_SOURCES
    src/main.cpp
    src/intro_skip.cpp
)

if(WIN32)
    add_executable(tabletennis WIN32 ${TABLETENNIS_SOURCES})
else()
    add_executable(tabletennis ${TABLETENNIS_SOURCES})
endif()

rexglue_setup_target(tabletennis)

# Apply the x64 FP exception-mask correction without modifying the installed SDK.
target_include_directories(tabletennis BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sdk-overrides")
if(TARGET tabletennis_recomp)
    target_include_directories(tabletennis_recomp BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sdk-overrides")
endif()

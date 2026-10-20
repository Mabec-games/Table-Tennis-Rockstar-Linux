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

rexglue_setup_target(tabletennis GPU_PLUGINS xenos)

# SDK 0.10.0 uses $ORIGIN on Linux but only stages runtime libraries on
# Windows/macOS. Keep the Linux executable's dependencies beside it too.
if(UNIX AND NOT APPLE)
    target_link_options(tabletennis PRIVATE "-Wl,--export-dynamic-symbol=Tabletennis_ProfileGraphics")
    add_custom_command(TARGET tabletennis POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:rex::runtime> $<TARGET_FILE_DIR:tabletennis>
        VERBATIM)
endif()

# The SDK's post-build staging copies its stock Xenos plugin over the corrected
# production plugin. Preserve the locally built skin-fix plugin when available.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TABLETENNIS_FIXED_XENOS_PLUGIN
        "${CMAKE_BINARY_DIR}/../gpu-fixed/out/librexgpu-xenos.so")
    if(EXISTS "${TABLETENNIS_FIXED_XENOS_PLUGIN}")
        add_custom_command(TARGET tabletennis POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TABLETENNIS_FIXED_XENOS_PLUGIN}"
                "$<TARGET_FILE_DIR:tabletennis>/librexgpu-xenos.so"
            COMMENT "Restoring the confirmed Linux skin shader fix"
            VERBATIM)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(tabletennis PRIVATE src/linux_memory_query.cpp)
    # Interpose the SDK's call too, not just calls originating in the executable.
    target_link_options(tabletennis PRIVATE
        "-Wl,--export-dynamic-symbol=_ZN3rex6memory12QueryProtectEPvRmRNS0_10PageAccessE")
    target_link_libraries(tabletennis PRIVATE ${CMAKE_DL_LIBS})
endif()

# The stock v0.10.0 application accepts directories only. Compile our BSD-licensed
# ISO-capable application adapter instead, without editing the installed SDK.
get_target_property(TABLETENNIS_HOST_SOURCES tabletennis SOURCES)
set(TABLETENNIS_SDK_APP "${REXGLUE_SHARE_DIR}/rex_app.cpp")
if(NOT TABLETENNIS_SDK_APP IN_LIST TABLETENNIS_HOST_SOURCES)
    message(FATAL_ERROR "SDK application source not found; cannot enable ISO support")
endif()
list(REMOVE_ITEM TABLETENNIS_HOST_SOURCES "${TABLETENNIS_SDK_APP}")
set_property(TARGET tabletennis PROPERTY SOURCES "${TABLETENNIS_HOST_SOURCES}")
target_sources(tabletennis PRIVATE sdk-overrides/rex_app.cpp)

# Apply the x64 FP exception-mask correction without modifying the installed SDK.
target_include_directories(tabletennis BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sdk-overrides")
if(TARGET tabletennis_recomp)
    target_include_directories(tabletennis_recomp BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sdk-overrides")
endif()

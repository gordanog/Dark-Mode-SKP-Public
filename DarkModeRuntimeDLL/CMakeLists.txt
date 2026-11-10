cmake_minimum_required(VERSION 3.20)
project(DarkModeRuntimeDLL CXX)

if(NOT WIN32)
  message(FATAL_ERROR "DarkModeRuntimeDLL supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

function(darkmode_configure_decoder)
  include(FetchContent)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  set(PROJECT_VERSION "5.0.6")
  set(BUILD_SHARED_LIBS OFF)
  set(BUILD_STATIC_LIBS ON)
  set(BUILD_STATIC_RUNTIME ON)
  set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
  set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
  set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
  set(CAPSTONE_BUILD_CSTEST OFF CACHE BOOL "" FORCE)
  set(CAPSTONE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(capstone
    URL https://github.com/capstone-engine/capstone/archive/refs/tags/5.0.6.tar.gz
    URL_HASH SHA256=240ebc834c51aae41ca9215d3190cc372fd132b9c5c8aa2d5f19ca0c325e28f9
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/capstone-build")
  FetchContent_MakeAvailable(capstone)
endfunction()
darkmode_configure_decoder()

add_library(DarkModeRuntimeDLL SHARED
  src/dark_mode_skp.cpp
)

if(WIN32)
  target_link_libraries(DarkModeRuntimeDLL PRIVATE
    user32
    gdi32
    capstone_static
  )
endif()

if(MSVC)
  set_property(TARGET DarkModeRuntimeDLL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
  target_compile_options(DarkModeRuntimeDLL PRIVATE
    /W4
    /WX-
    /MP
    $<$<CONFIG:Release>:/O2>
  )
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set_target_properties(DarkModeRuntimeDLL PROPERTIES
  OUTPUT_NAME "main"
  PREFIX ""
)

set(DARKMODE_EXTENSION_DLL "${CMAKE_SOURCE_DIR}/../DarkModeSKP/DarkModeSKP/main.dll")
option(DARKMODE_DEPLOY_PLUGIN_RUNTIME "Copy the runtime into the extension after building" ON)
if(DARKMODE_DEPLOY_PLUGIN_RUNTIME)
  add_custom_command(TARGET DarkModeRuntimeDLL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:DarkModeRuntimeDLL>"
      "${DARKMODE_EXTENSION_DLL}"
    COMMENT "Copying main.dll into the Dark Mode SKP extension"
  )
endif()
cmake_minimum_required(VERSION 3.20)
project(DarkModeRuntimeDLL CXX)

if(NOT WIN32)
  message(FATAL_ERROR "DarkModeRuntimeDLL supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(DarkModeRuntimeDLL SHARED
  src/dark_mode_skp.cpp
)

if(WIN32)
  target_link_libraries(DarkModeRuntimeDLL PRIVATE
    user32
    gdi32
  )
endif()

if(MSVC)
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
add_custom_command(TARGET DarkModeRuntimeDLL POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:DarkModeRuntimeDLL>"
    "${DARKMODE_EXTENSION_DLL}"
  COMMENT "Copying main.dll into the Dark Mode SKP extension"
)
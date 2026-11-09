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
option(DARKMODE_DEPLOY_PLUGIN_RUNTIME "Copy the runtime into the extension after building" ON)
if(DARKMODE_DEPLOY_PLUGIN_RUNTIME)
  add_custom_command(TARGET DarkModeRuntimeDLL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:DarkModeRuntimeDLL>"
      "${DARKMODE_EXTENSION_DLL}"
    COMMENT "Copying main.dll into the Dark Mode SKP extension"
  )
endif()

option(DARKMODE_BUILD_TOOLBAR_TITLE_TEST "Build the installed-Qt toolbar title rendering test" OFF)
set(DARKMODE_QT_TEST_DIR "" CACHE PATH "Directory containing SketchUp's Qt6 DLLs")
if(DARKMODE_BUILD_TOOLBAR_TITLE_TEST)
  include(CTest)
  add_executable(DarkModeToolbarTitleTest tests/test_floating_toolbar_title.cpp)
  target_link_libraries(DarkModeToolbarTitleTest PRIVATE user32 gdi32)
  add_test(NAME DarkModeToolbarTitleTest COMMAND DarkModeToolbarTitleTest "${DARKMODE_QT_TEST_DIR}")
  set_tests_properties(DarkModeToolbarTitleTest PROPERTIES SKIP_RETURN_CODE 77)
endif()
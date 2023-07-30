cmake_minimum_required(VERSION 3.6 FATAL_ERROR)

project(whenever_tray_core LANGUAGES CXX)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# hack for buggy CMake's FindwxWidgets script
if(DEFINED ENV_WX_CONFIG)
    set(ENV{WX_CONFIG} ${ENV_WX_CONFIG})
endif()

set(wxWidgets_USE_STATIC 1)

find_package(wxWidgets REQUIRED)

set(SRCS whenever_tray.cpp)

include(${wxWidgets_USE_FILE})

if(APPLE)
    # create bundle on apple compiles
    add_executable(whenever_tray MACOSX_BUNDLE ${SRCS})

    # Set a custom plist file for the app bundle - needed for Mac OS Retina display
    set_target_properties(whenever_tray PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(whenever_tray WIN32 ${SRCS} whenever_tray.exe.manifest)
endif()

target_link_libraries(whenever_tray PRIVATE ${wxWidgets_LIBRARIES})
cmake_minimum_required(VERSION 3.20)
project(awdshells LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT STATIC)
    set(STATIC OFF CACHE BOOL "Build static executable" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# define _WIN32_WINNT=0x0601 if is building for windows
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

include_directories(inc)
include_directories(3rdparty/asio)
include_directories(3rdparty/argparse)

add_executable(awdshells
    src/awdshells.cpp
    src/batch.cpp
    src/cli.cpp
    src/logger.cpp
    src/server.cpp
    src/session.cpp
    src/utils.cpp
)

if(STATIC)
    if(MSVC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
        target_compile_options(awdshells PRIVATE /MT)
        target_link_options(awdshells PRIVATE /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT)
    else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
        set(BUILD_SHARED_LIBS OFF)
        set(CMAKE_EXE_LINKER_FLAGS "-static")
    endif()
endif()


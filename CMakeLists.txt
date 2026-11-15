cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" GNARL_VERSION)
string(STRIP "${GNARL_VERSION}" GNARL_VERSION)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/CONTENT_VERSION" GNARL_CONTENT_VERSION)
string(STRIP "${GNARL_CONTENT_VERSION}" GNARL_CONTENT_VERSION)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
project(GNARL VERSION ${GNARL_VERSION} LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(GNARL_COPY_PLUGINS "Copy built plugins to ~/Library/Audio/Plug-Ins" OFF)
option(GNARL_BUILD_TESTS "Build tests" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)
FetchContent_Declare(juce
  URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.5.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(catch2
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(monocypher
  URL https://github.com/LoupVaillant/Monocypher/archive/refs/tags/4.0.3.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR do-not-add)
FetchContent_MakeAvailable(juce monocypher)

add_library(monocypher STATIC
  ${monocypher_SOURCE_DIR}/src/monocypher.c
  ${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c)
target_include_directories(monocypher PUBLIC ${monocypher_SOURCE_DIR}/src ${monocypher_SOURCE_DIR}/src/optional)

add_compile_definitions(GNARL_VERSION_STRING="${GNARL_VERSION}" GNARL_CONTENT_VERSION=${GNARL_CONTENT_VERSION})

add_subdirectory(src/dsp)
add_subdirectory(src/app)
add_subdirectory(src/plugins)
add_subdirectory(tools)
if(GNARL_BUILD_TESTS)
  FetchContent_MakeAvailable(catch2)
  enable_testing()
  add_subdirectory(tests)
endif()

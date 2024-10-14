cmake_minimum_required(VERSION 3.10)

project(libmagritte
    VERSION 1.0
    LANGUAGES CXX)

option(ENABLE_TESING "Building tests" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

enable_testing()

add_subdirectory(src)

if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()

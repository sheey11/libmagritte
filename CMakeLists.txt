cmake_minimum_required(VERSION 3.10)

project(magritte
    VERSION 1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -D_DEBUG -D_GLIBCXX_DEBUG")
# set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -D_DEBUG -D_GLIBCXX_DEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_TESTING "Building tests" OFF)

include_directories(${CMAKE_SOURCE_DIR}/include)
add_subdirectory(src)

if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()


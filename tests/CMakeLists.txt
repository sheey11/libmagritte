include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES "*.cpp")
add_executable(runTests ${TEST_SOURCES})
target_link_libraries(runTests PRIVATE libmagritte gtest_main)

add_test(NAME AllTests COMMAND runTests)


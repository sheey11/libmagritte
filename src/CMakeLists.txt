file(GLOB_RECURSE LIBRARY_SOURCES
    "*.cpp"
    "*/*.cpp"
)

add_library(magritte SHARED ${LIBRARY_SOURCES})


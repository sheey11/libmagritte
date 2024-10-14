file(GLOB_RECURSE LIBRARY_SOURCES
    "*.cpp"
    "*/*.cpp"
)

add_library(libmagritte ${LIBRARY_SOURCES})


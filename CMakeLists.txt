
CMAKE_MINIMUM_REQUIRED(VERSION 3.13...3.20)

project(histreamer)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else ()
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif ()

set(TOP_DIR ${PROJECT_SOURCE_DIR})

set(CMAKE_VERBOSE_MAKEFILE ON)

add_subdirectory(${DEV_TOP_DIR}/histreamer/engine engine.out)
add_subdirectory(${DEV_TOP_DIR}/histreamer/tests tests.out)

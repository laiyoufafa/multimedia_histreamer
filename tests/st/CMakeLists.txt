###############################
# find python3
find_package (Python3 COMPONENTS Interpreter)

IF(Python3_FOUND)
    SET(PYTHON ${Python3_EXECUTABLE})
ELSE()
    MESSAGE(FATAL_ERROR "No Python interpreter found")
ENDIF(Python3_FOUND)

###############################
# definitions

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

IF(UNIX OR MINGW)
ADD_DEFINITIONS(
    -g -ggdb
)
ADD_LINK_OPTIONS(
    -Wl,--export-all-symbols
)
ENDIF(UNIX OR MINGW)

ADD_DEFINITIONS(-D__STDC_FORMAT_MACROS)

IF(CMAKE_CL_64)
    ADD_DEFINITIONS(-DWIN64)
ENDIF(CMAKE_CL_64)

IF(MSVC)
  ADD_DEFINITIONS( -D_CRT_SECURE_NO_WARNINGS /source-charset:utf-8)
ENDIF(MSVC)

set(TOP_DIR ${CMAKE_SOURCE_DIR})

ADD_DEFINITIONS(
        -D__STDC_FORMAT_MACROS
        -DHST_PLUGIN_PATH="./"
)
if (WIN32)
    add_definitions( -DHST_PLUGIN_FILE_TAIL=".dll" )
else()
    add_definitions( -DHST_PLUGIN_FILE_TAIL=".so" )
endif()

###############################
# includes

INCLUDE_DIRECTORIES(BEFORE
        ${TOP_DIR}/3rdparty/testngpp2/include
        ${TOP_DIR}/demo/system_tests
        ${TOP_DIR}/histreamer/engine
        ${TOP_DIR}/histreamer/engine/scene/standard
        ${TOP_DIR}/histreamer/engine/plugin
        ${TOP_DIR}/3rdparty/ohos)
IF(MSVC)
#INCLUDE_DIRECTORIES(BEFORE ${CMAKE_SOURCE_DIR}/include/msinttypes)
ENDIF(MSVC)

###############################
# Generate Test Suite cxx files

SET(TEST_SUITES_DIR ${TOP_DIR}/histreamer/tests/st/gen_testsuites)
SET(TEST_GENERATOR ${TOP_DIR}/3rdparty/testngpp2/generator/testngppgen.py)
SET(TEST_SUITE_PREFIX st_)

SET(TESTSUITE_ENTRY ${TEST_SUITES_DIR}/AllTestSuites.cxx)
SET(TESTSUITE_SRCS ${TESTSUITE_ENTRY})

SET(UT_CASE_DIR ${TOP_DIR}/histreamer/tests/st)
file(GLOB_RECURSE UT_CASES_PATH ${UT_CASE_DIR}/*.h) # find all testcase .h files

FOREACH(CASE_PATH ${UT_CASES_PATH})

GET_FILENAME_COMPONENT(CASE ${CASE_PATH} NAME_WE)

SET(INPUT_H ${UT_CASE_DIR}/${CASE}.h)
SET(OUTPUT_CXX ${TEST_SUITES_DIR}/st_${CASE}.cxx)

SET(TESTSUITE_SRCS ${TESTSUITE_SRCS} ${OUTPUT_CXX})

ADD_CUSTOM_COMMAND(
  OUTPUT ${OUTPUT_CXX} ${TESTSUITE_ENTRY}
  COMMAND ${PYTHON} ${TEST_GENERATOR} -d ${TEST_SUITES_DIR} -p ${TEST_SUITE_PREFIX} ${INPUT_H}
  DEPENDS ${INPUT_H}
)

ENDFOREACH()

###############################
# src files
SET(TEST_PLUGIN_SRCS
        ${TOP_DIR}/histreamer/engine/plugin/core/plugin_manager.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/plugin_register.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/muxer.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/source.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/demuxer.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/base.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/audio_sink.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/codec.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/video_sink.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/plugin_core_utils.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/plugin_wrapper.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/output_sink.cpp
        ${TOP_DIR}/histreamer/engine/plugin/core/plugin_meta.cpp
        ${TOP_DIR}/histreamer/engine/foundation/osal/thread/mutex.cpp
        ${TOP_DIR}/histreamer/engine/foundation/osal/thread/scoped_lock.cpp)

###############################
# dependencies
if (MINGW)
    set(ffmpeg_lib_path ${TOP_DIR}/3rdparty/ffmpeg/windows/lib)
    set(sdl_lib_path ${TOP_DIR}/3rdparty/SDL2.0/windows/lib/x64)
    set(curl_lib_path ${TOP_DIR}/3rdparty/curl/lib/windows)
else ()
    set(CMAKE_CXX_FLAGS "-g -ggdb3 -O0 -std=c++11 -Wall")
    set(ffmpeg_lib_path /usr/local/ffmpeg/lib)
    set(sdl_lib_path /usr/lib/x86_64-linux-gnu/)
    set(curl_lib_path ${TOP_DIR}/3rdparty/curl/lib/linux)
endif ()

link_directories(
        ${ffmpeg_lib_path}
        ${sdl_lib_path}
        ${curl_lib_path}
)

if (${OHOS_LITE})
    # only for lite stream source player
    file(GLOB OSAL_THREAD_SRCS ${TOP_DIR}/histreamer/engine/foundation/osal/thread/*.cpp)
    set(TEST_HELPER_SRCS
            ${TOP_DIR}/histreamer/tests/st/helper/lite_player.cpp
            ${TOP_DIR}/histreamer/tests/st/helper/lite_stream_player.cpp
            ${OSAL_THREAD_SRCS}
            ${TOP_DIR}/histreamer/engine/foundation/osal/utils/util.cpp
            ${TOP_DIR}/histreamer/engine/scene/common/data_stream.cpp
            ${TOP_DIR}/histreamer/engine/scene/common/data_stream_impl.cpp
            )
else()
    set(TEST_HELPER_SRCS
            ${TOP_DIR}/histreamer/tests/st/helper/std_player.cpp
            )
endif ()

###############################
# targets
link_directories(${TOP_DIR}/3rdparty/testngpp2/lib)

ADD_EXECUTABLE(histreamer_st ${TESTSUITE_SRCS} ${TEST_HELPER_SRCS} ${3RDPARTY_SRCS} ${TEST_PLUGIN_SRCS})

IF(UNIX)
TARGET_LINK_LIBRARIES(histreamer_st testngpp2 dl)
ELSE()
TARGET_LINK_LIBRARIES(histreamer_st testngpp2 histreamer)
ENDIF()

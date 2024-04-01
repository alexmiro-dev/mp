
cmake_minimum_required(VERSION 3.27.7)
project(memory_pool)

include(FetchContent)

#================================================================================
# Preamble
#================================================================================
set(CMAKE_VERBOSE_MAKEFILE      true)
set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_BUILD_TYPE            Debug)
#set(CMAKE_GENERATOR             "Ninja")

add_compile_options(-fconcepts-diagnostics-depth=3 -fcoroutines)

#================================================================================
# Boost::ut
#================================================================================
FetchContent_Declare(
    boost_ut
    GIT_REPOSITORY https://github.com/boost-ext/ut.git
    GIT_TAG v2.0.1
)

FetchContent_GetProperties(boost_ut)
if(NOT boost_ut_POPULATED)
    FetchContent_Populate(boost_ut)
    add_subdirectory(${boost_ut_SOURCE_DIR} ${boost_ut_BINARY_DIR})
endif()

add_subdirectory(src)


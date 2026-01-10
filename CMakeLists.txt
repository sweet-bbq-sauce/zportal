cmake_minimum_required(VERSION 3.23)
cmake_policy(SET CMP0135 NEW)
project(zportal
    VERSION 0.1.4
    DESCRIPTION "IP4 tunneling through socks"
    HOMEPAGE_URL "https://github.com/sweet-bbq-sauce/zportal"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${PROJECT_SOURCE_DIR}/cmake/install.cmake")
include(FetchContent)

add_subdirectory("${PROJECT_SOURCE_DIR}/src")
add_subdirectory("${PROJECT_SOURCE_DIR}/app")

option(BUILD_TESTS "Build gtests" ON)

if(BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest
        GIT_TAG v1.17.0
    )
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)

    add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
endif()
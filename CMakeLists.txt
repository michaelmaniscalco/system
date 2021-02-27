cmake_minimum_required(VERSION 3.16.3)
set(CMAKE_CXX_STANDARD 20)

project(work_contract)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message("*** Build type not set.  defaulting to Release")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(
        -g
        -O0
        -march=native
    )
else()
    add_compile_options(
        -O3
        -march=native
    )
endif()


option(WORKCONTRACT_BUILD_DEMO "Build the CLI demo" OFF)

include(FetchContent)

# type_safe
FetchContent_Declare(
    type_safe
    GIT_REPOSITORY https://github.com/foonathan/type_safe.git
    GIT_TAG main
    SOURCE_DIR        "${CMAKE_BINARY_DIR}/type_safe-src"
    BINARY_DIR        "${CMAKE_BINARY_DIR}/type_safe-build"
    INSTALL_DIR       "${CMAKE_BINARY_DIR}"
    INSTALL_COMMAND   ""
)
FetchContent_MakeAvailable(type_safe)
FetchContent_GetProperties(type_safe)
set(_type_safe_include_dir "${CMAKE_BINARY_DIR}/type_safe-src/include/")

add_subdirectory(src)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


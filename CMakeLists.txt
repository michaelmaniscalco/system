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

include("./scripts/fetch_dependencies.cmake")

fetch_dependency("https://github.com/foonathan/type_safe.git;b9138d8a26ea9bbab965f87ee925f53fde025fd9")
fetch_dependency("https://github.com/ericniebler/range-v3.git;3d6e6f56e5e1a3ec4befcc7695504ea23e1d52ab")

add_subdirectory(src)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)



add_library(system
    ./threading/thread_pool.cpp
    ./system.cpp
)

target_link_libraries(system)

target_include_directories(system
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        $<BUILD_INTERFACE:${_system_include_dir}>
        $<INSTALL_INTERFACE:include/system>
        ${_range-v3_src_path}/include
)

target_compile_features(system PUBLIC cxx_std_20)


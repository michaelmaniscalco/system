find_library(LIBCXX_LIB c++)
find_package(Threads)
find_library(LIBCXXABI_LIB c++abi)

link_libraries(
    ${LIBCXX_LIB}
    ${LIBCXXABI_LIB}
)

add_library(threading
    thread_pool.cpp
)

target_link_libraries(threading ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(threading
    PUBLIC
        $<BUILD_INTERFACE:${_threading_include_dir}>
        $<INSTALL_INTERFACE:include/threading>
)

target_compile_features(threading PUBLIC cxx_std_20)

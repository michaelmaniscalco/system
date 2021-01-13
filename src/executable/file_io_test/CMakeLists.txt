find_package(Threads)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(file_io_test main.cpp)

target_link_libraries(file_io_test 
PRIVATE
    ${CMAKE_THREAD_LIBS_INIT} 
    work_contract
)
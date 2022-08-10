
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(performance_test main.cpp)

target_link_libraries(performance_test 
PRIVATE
    system
)
add_executable(performance_test main.cpp)

target_link_libraries(performance_test 
PRIVATE
    system
)
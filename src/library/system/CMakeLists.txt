
add_library(system
    ./work_contract/work_contract_group.cpp
    ./work_contract/work_contract.cpp
    ./work_contract/alertable_work_contract.cpp
    ./threading/thread_pool.cpp
)

target_link_libraries(system)

message(STATUS "path = ${CMAKE_SOURCE_DIR}/src")

target_include_directories(system
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        $<BUILD_INTERFACE:${_system_include_dir}>
        $<INSTALL_INTERFACE:include/system>
)

target_compile_features(system PUBLIC cxx_std_20)


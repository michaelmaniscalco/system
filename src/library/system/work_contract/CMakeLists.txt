find_library(LIBCXX_LIB c++)
find_package(Threads)
find_library(LIBCXXABI_LIB c++abi)

link_libraries(
    ${LIBCXX_LIB}
    ${LIBCXXABI_LIB}
)

add_library(work_contract
    work_contract_group.cpp
    work_contract.cpp
)

target_link_libraries(work_contract threading)

target_include_directories(work_contract
    PUBLIC
        $<BUILD_INTERFACE:${_work_contract_include_dir}>
        $<INSTALL_INTERFACE:include/work_contract>
)

target_compile_features(work_contract PUBLIC cxx_std_20)

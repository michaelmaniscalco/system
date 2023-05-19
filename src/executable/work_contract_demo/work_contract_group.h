#pragma once

#include <range/v3/view/enumerate.hpp>

#include <memory>
#include <cstdint>
#include <atomic>


class work_contract;


class work_contract_group
{
public:

    work_contract_group
    (
        std::int64_t
    );

    work_contract create_contract
    (
        std::function<void()>
    );

    work_contract create_contract
    (
        std::function<void()>,
        std::function<void()>
    );

    void service_contracts();

private:

    friend class work_contract;

    struct contract
    {
        static auto constexpr surrender_flag    = 0x00000004;
        static auto constexpr execute_flag      = 0x00000002;
        static auto constexpr invoke_flag       = 0x00000001;
    
        std::function<void()>       work_;
        std::function<void()>       surrender_;
        std::atomic<std::int32_t>   flags_;
    };

    void invoke
    (
        work_contract const &
    );

    void surrender
    (
        work_contract const &
    );

    std::int64_t decrement_contract_count_left_preference(std::int64_t);

    std::int64_t decrement_contract_count_right_preference(std::int64_t);

    void process_contract(std::int64_t);

    void increment_contract_count(std::int64_t);

    union alignas(8) invocation_counter
    {
        invocation_counter():u64_(){}

        std::atomic<std::uint64_t> u64_;
        struct
        {
            std::uint32_t left_;
            std::uint32_t right_;
        } u32_;
    };

    static_assert(sizeof(invocation_counter) == sizeof(std::uint64_t));

    std::vector<invocation_counter>         invocationCounter_;

    std::vector<contract>                   contracts_;

    std::int64_t                            firstContractIndex_;

    std::mutex                              mutex_;

    std::atomic<std::int32_t>               nextAvail_;
    
    std::atomic<std::uint64_t>              preferenceFlags_;

};


#include "./work_contract.h"


//=============================================================================
work_contract_group::work_contract_group
(
    std::int64_t capacity
):
    invocationCounter_(capacity - 1), 
    contracts_(capacity),
    firstContractIndex_(capacity - 1)
{
    for (auto && [index, contract] : ranges::v3::views::enumerate(contracts_))
        contract.flags_ = (index + 1);
    contracts_.back().flags_ = ~0;
    nextAvail_ = 0;
}


//=============================================================================
auto work_contract_group::create_contract
(
    std::function<void()> function
) -> work_contract
{
    return create_contract(function, nullptr);
}


//=============================================================================
auto work_contract_group::create_contract
(
    std::function<void()> function,
    std::function<void()> surrender
) -> work_contract
{
    std::lock_guard lockGuard(mutex_);
    auto contractId = nextAvail_.load();
    if (contractId == ~0)
        return {}; // no free contracts
    auto & contract = contracts_[contractId];
    nextAvail_ = contract.flags_.load();
    contract.flags_ = 0;
    contract.work_ = function;
    contract.surrender_ = surrender;
    return work_contract(this, contractId);
}


//=============================================================================
inline void work_contract_group::surrender
(
    work_contract const & workContract
)
{
    auto contractId = workContract.get_id();
    if ((contracts_[contractId].flags_.fetch_or((contract::surrender_flag | contract::invoke_flag)) & (contract::execute_flag | contract::invoke_flag)) == 0)
        increment_contract_count(contractId);
}


//=============================================================================
inline void work_contract_group::invoke
(
    work_contract const & workContract
)
{
    auto contractId = workContract.get_id();
    if ((contracts_[contractId].flags_.fetch_or(contract::invoke_flag) & (contract::execute_flag | contract::invoke_flag)) == 0)
        increment_contract_count(contractId);
}


//=============================================================================
inline void work_contract_group::increment_contract_count
(
    std::int64_t current
)
{
    current += firstContractIndex_;
    while (current)
    {
        auto addend = ((current-- & 1ull) ? 0x0000000000000001ull : 0x0000000100000000ull);
        invocationCounter_[current >>= 1].u64_ += addend;
    }
}


//=============================================================================
inline void work_contract_group::service_contracts
(
)
{
    std::uint64_t preferenceFlags = preferenceFlags_++;
    if (auto parent = (preferenceFlags & 1) ? decrement_contract_count_right_preference(0) : decrement_contract_count_left_preference(0))   
    {
        while (parent < firstContractIndex_) 
        {
            parent = (parent * 2) + ((preferenceFlags & 1) ? decrement_contract_count_right_preference(parent) : decrement_contract_count_left_preference(parent));
            preferenceFlags >>= 1;
        }
        process_contract(parent);
    }
}


//=============================================================================
inline std::int64_t work_contract_group::decrement_contract_count_right_preference
(
    std::int64_t parent
)
{   
    auto & invocationCounter = invocationCounter_[parent].u64_;
    auto expected = invocationCounter.load();
    auto addend = (expected & 0xffffffff00000000ull) ? 0x0000000100000000ull : 0x0000000000000001ull;
    while ((expected != 0) && (!invocationCounter.compare_exchange_strong(expected, expected - addend)))
        addend = (expected & 0xffffffff00000000ull) ? 0x0000000100000000ull : 0x0000000000000001ull;
    return expected ? (1 + (addend > 0x00000000ffffffffull)) : 0;
}


//=============================================================================
inline std::int64_t work_contract_group::decrement_contract_count_left_preference
(
    std::int64_t parent
)
{  
    auto & invocationCounter = invocationCounter_[parent].u64_;
    auto expected = invocationCounter.load();
    auto addend = (expected & 0x00000000ffffffffull) ? 0x0000000000000001ull : 0x0000000100000000ull;
    while ((expected != 0) && (!invocationCounter.compare_exchange_strong(expected, expected - addend)))
        addend = (expected & 0x00000000ffffffffull) ? 0x0000000000000001ull : 0x0000000100000000ull;
    return expected ? (1 + (addend > 0x00000000ffffffffull)) : 0;
}


//=============================================================================
inline void work_contract_group::process_contract
(
    std::int64_t parent
)
{
    auto contractId = (parent - firstContractIndex_);
    auto & contract = contracts_[contractId];
    auto & flags = contract.flags_;
    if ((++flags & contract::surrender_flag) != contract::surrender_flag)
    {
        contract.work_();
        if (((flags -= contract::execute_flag) & contract::invoke_flag) == contract::invoke_flag)
            increment_contract_count(contractId);
        return;
    }

    contract.surrender_();
    std::lock_guard lockGuard(mutex_);
    flags = nextAvail_.load();
    nextAvail_ = contractId;
    contract.work_ = contract.surrender_ = nullptr;
}

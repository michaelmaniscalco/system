#pragma once

#include <range/v3/view/enumerate.hpp>

#include <memory>
#include <cstdint>
#include <atomic>


namespace maniscalco::system 
{

    class work_contract;


    class work_contract_group
    {
    public:

        class surrender_token;

        work_contract_group
        (
            std::int64_t
        );

        ~work_contract_group();

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

        std::size_t get_capacity() const;

    private:

        friend class work_contract;
        friend class surrender_token;

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
            invocation_counter():u64_(){static_assert(sizeof(*this) == sizeof(std::uint64_t));}
            std::atomic<std::uint64_t> u64_;
            struct
            {
                std::uint32_t left_;
                std::uint32_t right_;
            } u32_;
        };

        std::vector<invocation_counter>                 invocationCounter_;

        std::vector<contract>                           contracts_;

        std::vector<std::shared_ptr<surrender_token>>   surrenderToken_;

        std::int64_t                                    firstContractIndex_;

        std::mutex                                      mutex_;

        std::atomic<std::uint32_t>                      nextAvail_;
        
        std::atomic<std::uint64_t>                      preferenceFlags_;

    }; // class work_contract_group



    class work_contract_group::surrender_token
    {
    public:

        surrender_token
        (
            work_contract_group *
        );
        
        std::mutex mutex_;
        work_contract_group * workContractGroup_{};

        bool invoke(work_contract const &);

        void orphan();
    };

} // namespace maniscalco::system


#include "./work_contract.h"


//=============================================================================
inline maniscalco::system::work_contract_group::work_contract_group
(
    std::int64_t capacity
):
    invocationCounter_(capacity - 1), 
    contracts_(capacity),
    surrenderToken_(capacity),
    firstContractIndex_(capacity - 1)
{
    for (auto && [index, contract] : ranges::v3::views::enumerate(contracts_))
        contract.flags_ = (index + 1);
    contracts_.back().flags_ = ~0;
    nextAvail_ = 0;
}


//=============================================================================
inline maniscalco::system::work_contract_group::~work_contract_group
(
)
{
    std::lock_guard lockGuard(mutex_);
    for (auto & surrenderToken : surrenderToken_)
        if ((bool)surrenderToken)
            surrenderToken->orphan();
}


//=============================================================================
inline auto maniscalco::system::work_contract_group::create_contract
(
    std::function<void()> function
) -> work_contract
{
    return create_contract(function, nullptr);
}


//=============================================================================
inline auto maniscalco::system::work_contract_group::create_contract
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
    auto surrenderToken = surrenderToken_[contractId] = std::make_shared<surrender_token>(this);
    return {this, surrenderToken, contractId};
}


//=============================================================================
inline void maniscalco::system::work_contract_group::surrender
(
    work_contract const & workContract
)
{
    static auto constexpr flags_to_set = (contract::surrender_flag | contract::invoke_flag);
    static auto constexpr flags_mask = (contract::execute_flag | contract::invoke_flag);
    auto contractId = workContract.get_id();
    if ((contracts_[contractId].flags_.fetch_or(flags_to_set) & flags_mask) == 0)
        increment_contract_count(contractId);
}


//=============================================================================
inline void maniscalco::system::work_contract_group::invoke
(
    work_contract const & workContract
)
{
    static auto constexpr flags_to_set = contract::invoke_flag;
    static auto constexpr flags_mask = (contract::execute_flag | contract::invoke_flag);
    auto contractId = workContract.get_id();
    if ((contracts_[contractId].flags_.fetch_or(flags_to_set) & flags_mask) == 0)
        increment_contract_count(contractId);
}


//=============================================================================
inline void maniscalco::system::work_contract_group::increment_contract_count
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
inline void maniscalco::system::work_contract_group::service_contracts
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
inline std::int64_t maniscalco::system::work_contract_group::decrement_contract_count_right_preference
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
inline std::int64_t maniscalco::system::work_contract_group::decrement_contract_count_left_preference
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
inline void maniscalco::system::work_contract_group::process_contract
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

    if (contract.surrender_)
        std::exchange(contract.surrender_, nullptr)();
    std::lock_guard lockGuard(mutex_);
    flags = nextAvail_.load();
    nextAvail_ = contractId;
    contract.work_ = nullptr;
    surrenderToken_[contractId] = {};
}


//=============================================================================
inline std::size_t maniscalco::system::work_contract_group::get_capacity
(
) const
{
    return contracts_.size();
}


//=============================================================================
inline maniscalco::system::work_contract_group::surrender_token::surrender_token
(
    work_contract_group * workContractGroup
):
    workContractGroup_(workContractGroup)
{
}


//=============================================================================
inline bool maniscalco::system::work_contract_group::surrender_token::invoke
(
    work_contract const & workContract
)
{
    std::lock_guard lockGuard(mutex_);
    if (auto workContractGroup = std::exchange(workContractGroup_, nullptr); workContractGroup != nullptr)
    {
        workContractGroup->surrender(workContract);
        return true;
    }
    return false;
}


//=============================================================================
inline void maniscalco::system::work_contract_group::surrender_token::orphan
(
)
{
    std::lock_guard lockGuard(mutex_);
    workContractGroup_ = nullptr;
}

#include "./work_contract_group.h"
#include <include/invoke_from_weak_ptr.h>


//=====================================================================================================================
auto maniscalco::system::work_contract_group::create
(
    contract_service_handler contractRequiresServiceHandler
) -> std::shared_ptr<work_contract_group> 
{
    return std::shared_ptr<work_contract_group>(new work_contract_group(contractRequiresServiceHandler));
}


//=====================================================================================================================
maniscalco::system::work_contract_group::work_contract_group
(
    contract_service_handler contractRequiresServiceHandler
):
    sharedState_(std::make_shared<shared_state>())
{
    sharedState_->contractRequiresServiceHandler_ = contractRequiresServiceHandler;
}


//=====================================================================================================================
maniscalco::system::work_contract_group::~work_contract_group
(
) 
{
}


//=====================================================================================================================
bool maniscalco::system::work_contract_group::get_service_requested
(
) const
{
    std::uint64_t k = 0;
    for (auto const & flags : sharedState_->contractStateFlags_)
        k |= flags;
    return (k != 0);
}


/*
//======================================================================================================================
template <>
inline void maniscalco::system::work_contract_group::service_contract_by_index<32>
(
    // experiment with loop unrolling through specialization
    contract_info *,
    std::atomic<std::uint64_t> &
)
{
}


//======================================================================================================================
template <std::size_t N>
inline void maniscalco::system::work_contract_group::service_contract_by_index
(
    // experiment with loop unrolling through specialization
    contract_info * currentContract,
    std::atomic<std::uint64_t> & flags
)
{
    static constexpr auto need_service_bit = (0x01ull << (N << 1));
    static constexpr auto is_being_serviced_bit = (0x02ull << (N << 1));
    static constexpr auto bit_mask = (need_service_bit | is_being_serviced_bit);

    auto expected = flags.load();
    if (need_service_bit > expected)
        return;

    while ((expected & bit_mask) == need_service_bit)
    {
        auto const desired = ((expected & ~need_service_bit) | is_being_serviced_bit);
        if (flags.compare_exchange_strong(expected, desired))
        {
            if (currentContract->contractStatus_ == contract_info::contract_status::unsubscribed)
            {
                // contract has ended
                if (currentContract->endContractHandler_)
                    currentContract->endContractHandler_();
                currentContract->contractStatus_ = contract_info::contract_status::none;
            }
            else
            {
                currentContract->contractHandler_();
            }
            flags &= ~is_being_serviced_bit;
            break;
        }
    }
    service_contract_by_index<N + 1>(currentContract + 1, flags);
}


//=====================================================================================================================
void maniscalco::system::work_contract_group::service_contracts
(
    // an experimental variation of this function which uses template specialization for loop unrolling
) 
{
    auto n = 0ull;
    for (auto & flags : sharedState_->contractStateFlags_) 
        service_contract_by_index<0>(&sharedState_->contracts_[n << 5], flags);
}
*/


//=====================================================================================================================
void maniscalco::system::work_contract_group::service_contracts
(
) 
{
    auto currentContract = sharedState_->contracts_.data();
    for (auto & flags : sharedState_->contractStateFlags_) 
    {
        std::uint64_t need_service_bit = 0x01;
        std::uint64_t is_being_serviced_bit = 0x02;
        auto endContract = currentContract + 32;
        while ((need_service_bit <= flags) && (currentContract < endContract)) 
        {
            auto expected = flags.load();
            while ((expected & (need_service_bit | is_being_serviced_bit)) == need_service_bit)
            {
                auto desired = ((expected & ~need_service_bit) | is_being_serviced_bit);
                if (flags.compare_exchange_strong(expected, desired))
                {
                    if (currentContract->contractStatus_ == contract_info::contract_status::unsubscribed)
                    {
                        // contract has ended
                        if (currentContract->endContractHandler_)
                            currentContract->endContractHandler_();
                        currentContract->contractStatus_ = contract_info::contract_status::none;
                    }
                    else
                    {
                        currentContract->contractHandler_();
                    }
                    flags &= ~is_being_serviced_bit;
                    break;
                }
            }
            need_service_bit <<= 2;
            is_being_serviced_bit <<= 2;
            ++currentContract;
        }
        currentContract = endContract;
    }
}



/*
//=====================================================================================================================
void maniscalco::system::work_contract_group::service_contracts
(
) 
{
    auto currentContract = sharedState_->contracts_.data();
    for (auto & flags : sharedState_->contractStateFlags_) 
    {
        std::uint64_t need_service_bit = 0x01;
        auto endContract = currentContract + 32;
        while ((need_service_bit <= flags) && (currentContract < endContract)) 
        {
            if (flags & need_service_bit)
            {
                std::uint64_t is_being_serviced_bit = (need_service_bit << 1);
                auto current = (flags & ~(need_service_bit | is_being_serviced_bit));
                auto expected = (current | need_service_bit);
                auto desired = (current | is_being_serviced_bit);
                if (flags.compare_exchange_strong(expected, desired)) 
                {
                    if (currentContract->contractStatus_ == contract_info::contract_status::unsubscribed)
                    {
                        // contract has ended
                        if (currentContract->endContractHandler_)
                            currentContract->endContractHandler_();
                        currentContract->contractStatus_ = contract_info::contract_status::none;
                    }
                    else
                    {
                        currentContract->contractHandler_();
                    }
                    flags &= ~is_being_serviced_bit;
                }
            }
            need_service_bit <<= 2;
            ++currentContract;
        }
        currentContract = endContract;
    }
}
*/

    

//=====================================================================================================================
auto maniscalco::system::work_contract_group::create_contract
(
    contract_configuration_type contractConfiguration
) -> std::optional<work_contract>
{
    if (!contractConfiguration.contractHandler_)
        return std::nullopt; // disallow the nonsensical to prevent the need to test for validity constantly

    std::uint64_t contractIndex = 0;
    for (; contractIndex < capacity; ++contractIndex)
    {
        using status = contract_info::contract_status; 
        auto expected = status::none;
        if (sharedState_->contracts_[contractIndex].contractStatus_.compare_exchange_strong(expected, status::subscribed))
            break;
    }
    if (contractIndex == capacity)
        return std::nullopt; // at capacity

    sharedState_->contracts_[contractIndex].contractHandler_ = contractConfiguration.contractHandler_;
    sharedState_->contracts_[contractIndex].endContractHandler_ = contractConfiguration.endContractHandler_;
    
    return (sharedState_->contractRequiresServiceHandler_) ? 
        work_contract({ // non optimized version if callback is provided
            [contractIndex, sharedState = sharedState_] // contract handler
            (
            )
            {
                auto const contractServiceFlag =  (1ull << ((contractIndex & 0x1f) << 1));
                auto & contractFlags = sharedState->contractStateFlags_[contractIndex >> 5];
                if ((contractFlags.fetch_or(contractServiceFlag) & contractServiceFlag) == 0)
                    sharedState->contractRequiresServiceHandler_();
            },
            [contractIndex, sharedState = sharedState_] // unsubscribe handler
            (
            ) mutable
            {
                auto const contractServiceFlag =  (1ull << ((contractIndex & 0x1f) << 1));
                auto & contractFlags = sharedState->contractStateFlags_[contractIndex >> 5];
                sharedState->contracts_[contractIndex].contractStatus_ = contract_info::contract_status::unsubscribed;
                if ((contractFlags.fetch_or(contractServiceFlag) & contractServiceFlag) == 0)
                    sharedState->contractRequiresServiceHandler_();
            }}) :
        work_contract({ // optimized version if no callback is provided
            [contractIndex, sharedState = sharedState_] // contract handler
            (
            )
            {
                auto const contractServiceFlag =  (1ull << ((contractIndex & 0x1f) << 1));
                auto & contractFlags = sharedState->contractStateFlags_[contractIndex >> 5];
                contractFlags |= contractServiceFlag;
            },
            [contractIndex, sharedState = sharedState_] // unsubscribe handler
            (
            ) mutable
            {
                auto const contractServiceFlag =  (1ull << ((contractIndex & 0x1f) << 1));
                auto & contractFlags = sharedState->contractStateFlags_[contractIndex >> 5];
                sharedState->contracts_[contractIndex].contractStatus_ = contract_info::contract_status::unsubscribed;
                contractFlags |= contractServiceFlag;
            }});   
}

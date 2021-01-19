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
bool maniscalco::system::work_contract_group::get_service_requested
(
) const
{
    std::uint64_t k = 0;
    for (auto const & flags : sharedState_->contractStateFlags_)
        k |= flags;
    return (k != 0);
}


//=====================================================================================================================
void maniscalco::system::work_contract_group::service_contracts
(
) 
{
    std::exception_ptr currentException;
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
                        {
                            try
                            {
                                currentContract->endContractHandler_();
                            }
                            catch (...)
                            {
                                currentException = std::current_exception();
                            }
                        }
                        currentContract->contractStatus_ = contract_info::contract_status::none;
                    }
                    else
                    {
                        // contract needs servicing
                        try
                        {
                            currentContract->contractHandler_();
                        }
                        catch (...)
                        {
                            currentException = std::current_exception();
                        }
                    }
                    flags &= ~is_being_serviced_bit;
                    if (currentException)
                        std::rethrow_exception(currentException);
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

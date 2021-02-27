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
template <std::size_t N>
void maniscalco::system::work_contract_group::service_contract
(
    std::atomic<element_type> & flags,
    contract_info * contractInfo
)
{
    static auto constexpr contract_shift_offset = ((N % contracts_per_element_type) * bits_per_contract);
    static auto constexpr need_service_bit = (0x01ull << contract_shift_offset);
    static auto constexpr is_being_serviced_bit = (0x02ull << contract_shift_offset);
    static auto constexpr state_bits = (0x0cull << contract_shift_offset);

    std::exception_ptr currentException;
    auto expected = flags.load();
    while ((expected & (need_service_bit | is_being_serviced_bit)) == need_service_bit) // while need_service_bit = 1 and is_being_serviced_bit = 0
    {
        auto desired = ((expected & ~need_service_bit) | is_being_serviced_bit); // try need_service_bit -> 0 and is_being_serviced_bit -> 1
        if (flags.compare_exchange_strong(expected, desired))
        {
            // claimed contract for servicing
            // look at state bits to see what to do with contract
            auto state = ((flags >> (contract_shift_offset + 2)) & 0x03);
            if (state == 0x01)
            {
                // normal invokation of contract
                try
                {
                    contractInfo->contractHandler_();
                }
                catch (...)
                {
                    currentException = std::current_exception();
                }
            }
            else
            {
                if (state == 0x03)
                {
                    // surrender invokation of contract
                    try
                    {
                       contractInfo->endContractHandler_();
                    }
                    catch (...)
                    {
                        currentException = std::current_exception();
                    }
                    // clear state flags to unused (zero)
                    flags &= ~state_bits;
                }
            }
            // finally, clear is_being_serviced_bit (that this thread set)
            flags &= ~is_being_serviced_bit;
            if (currentException)
                std::rethrow_exception(currentException);
            return; // done
        }
    }
}


//=====================================================================================================================
template <std::size_t ... N>
void maniscalco::system::work_contract_group::service_contracts
(
    std::atomic<element_type> & flags,
    contract_info * contractInfo,
    std::index_sequence<N ...>
) 
{
    (service_contract<N>(flags, contractInfo + N), ...);
}


//=====================================================================================================================
void maniscalco::system::work_contract_group::service_contracts
(
) 
{
    auto contractInfo = sharedState_->contracts_.data();
    for (auto & flags : sharedState_->contractStateFlags_) 
    {
        if (flags)
            service_contracts(flags, contractInfo, std::make_index_sequence<contracts_per_element_type>());
        contractInfo += contracts_per_element_type;
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
    bool claimedSlot = false;
    while (contractIndex < capacity)
    {
        auto & element = sharedState_->contractStateFlags_[contractIndex / contracts_per_element_type];
        auto bitMask = ((1ull << bits_per_contract) - 1);
        auto shiftToContract = (bits_per_contract * (contractIndex % contracts_per_element_type));
        bitMask <<= shiftToContract;
        auto flagToSet = (0x04ull << shiftToContract);
        auto expected = element.load();
        while ((!claimedSlot) && ((expected & bitMask) == 0))
        {
            auto desired = (expected | flagToSet);
            claimedSlot = element.compare_exchange_strong(expected, desired);
        }
        if (claimedSlot)
            break;
        ++contractIndex;
    }
    if (!claimedSlot)
        return std::nullopt; // at capacity

    sharedState_->contracts_[contractIndex].contractHandler_ = contractConfiguration.contractHandler_;
    sharedState_->contracts_[contractIndex].endContractHandler_ = contractConfiguration.endContractHandler_ ? contractConfiguration.endContractHandler_ : [](){};
    
    auto shiftToContract = (bits_per_contract * (contractIndex % contracts_per_element_type));
    auto invokeFlags = (0x01ull << shiftToContract);
    auto surrenderFlags = (0x09ull << shiftToContract);
    auto * flags = (sharedState_->contractStateFlags_.data() + (contractIndex / contracts_per_element_type));


    return /*(sharedState_->contractRequiresServiceHandler_) ? 
        work_contract({ // non optimized version if callback is provided
            [contractIndex, sharedState = sharedState_] // contract handler
            (
            )
            {
                auto shiftToContract = (bits_per_contract * (contractIndex % contracts_per_element_type));
                auto contractServiceFlag = (0x01ull << shiftToContract);
                auto & element = (sharedState->contractStateFlags_[contractIndex / contracts_per_element_type]);
                if ((element.fetch_or(contractServiceFlag) & contractServiceFlag) == 0)
                    sharedState->contractRequiresServiceHandler_();
            },
            [contractIndex, sharedState = sharedState_] // unsubscribe handler
            (
            ) mutable
            {
                auto shiftToContract = (bits_per_contract * (contractIndex % contracts_per_element_type));
                auto contractServiceFlag = (0x09ull << shiftToContract);
                auto & element = (sharedState->contractStateFlags_[contractIndex / contracts_per_element_type]);
                if ((element.fetch_or(contractServiceFlag) & contractServiceFlag) == 0)
                    sharedState->contractRequiresServiceHandler_();
            }}) :
            */
        work_contract({.invokeFlags_ = invokeFlags, .surrenderFlags_ = surrenderFlags, .flags_ = flags});   
}

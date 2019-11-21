#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <optional>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <atomic>
#include <utility>

#include "./work_contract.h"


namespace maniscalco::system 
{

    class work_contract_group final
        : public std::enable_shared_from_this<work_contract_group> 
    {
    public:
        
        using contract_service_handler = std::function<void()>;
    
        using contract_handler = std::function<void()>;
        using end_contract_handler = std::function<void()>;
        
        struct contract_configuration_type
        {
            contract_handler        contractHandler_;
            end_contract_handler    endContractHandler_;
        };
        
        static std::shared_ptr<work_contract_group> create
        (
            contract_service_handler
        );

        ~work_contract_group();

        std::optional<work_contract> create_contract
        (
            contract_configuration_type
        );

        void service_contracts();

        bool get_service_requested() const;
        
        static constexpr std::uint64_t get_capacity()
        {
            return capacity;
        }

    protected:

    private:

        static std::uint64_t constexpr capacity = 1024;
        using contract_state_flags = std::array<std::atomic<std::uint64_t>, 2 * ((capacity + 63) / 64)>;

        work_contract_group
        (
            contract_service_handler
        );

        void on_end_contract
        (
            std::uint64_t
        );

        struct contract_info
        {
            enum class contract_status : std::uint32_t
            {
                none = 0,
                subscribed = 1,
                unsubscribed = 2
            };
            contract_info():contractStatus_(contract_status::none), contractHandler_(), errorHandler_(), endContractHandler_(){}
            std::atomic<contract_status>        contractStatus_;
            work_contract::contract_handler     contractHandler_;
            std::function<void()>               errorHandler_;
            work_contract::end_contract_handler endContractHandler_;
        };

        struct shared_state
        {
            contract_state_flags                    contractStateFlags_;
            contract_service_handler                contractRequiresServiceHandler_;
            std::array<contract_info, capacity>     contracts_;
        };

        std::shared_ptr<shared_state>               sharedState_;


        template <std::size_t> 
        void service_contract_by_index
        (    
            contract_info *,
            std::atomic<std::uint64_t> &
        );

    }; // class work_contract_group

} // namespace maniscalco::system


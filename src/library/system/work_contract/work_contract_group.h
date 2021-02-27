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

        ~work_contract_group() = default;

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

        static std::uint64_t constexpr capacity = (1 << 16);

        using element_type = std::uint64_t;
        static auto constexpr bits_per_byte = 8;
        static auto constexpr bits_per_element_type = (sizeof(element_type) * bits_per_byte);
        static auto constexpr bits_per_contract = 4; // must be power of two
        static auto constexpr contracts_per_element_type = (bits_per_element_type / bits_per_contract);
        using contract_state_flags = std::array<std::atomic<element_type>, (capacity + contracts_per_element_type - 1) / contracts_per_element_type>;


        struct contract_info
        {
            std::function<void()>               contractHandler_;
            std::function<void()>               endContractHandler_;
        };

        struct shared_state
        {
            contract_state_flags                    contractStateFlags_;
            contract_service_handler                contractRequiresServiceHandler_;
            std::array<contract_info, capacity>     contracts_;
        };

        template <std::size_t N>
        void service_contract
        (
            std::atomic<element_type> &,
            contract_info *
        );

        template <std::size_t ... N>
        void service_contracts
        (
            std::atomic<element_type> &,
            contract_info *,
            std::index_sequence<N ...>
        );

        work_contract_group
        (
            contract_service_handler
        );

        void on_end_contract
        (
            std::uint64_t
        );


        std::shared_ptr<shared_state>               sharedState_;

    }; // class work_contract_group

} // namespace maniscalco::system

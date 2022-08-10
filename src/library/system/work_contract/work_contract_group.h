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
#include "./alertable_work_contract.h"


namespace maniscalco::system 
{

    class work_contract_group final
        : public std::enable_shared_from_this<work_contract_group> 
    {
    public:
        
        using contract_service_handler = std::function<void()>;
        using contract_handler = std::function<void()>;
        using end_contract_handler = std::function<void()>;
        
        static std::uint64_t constexpr default_capacity = (1 << 16);

        struct configuration
        {
            std::size_t                 capacity_{default_capacity};
        };
        
        static std::shared_ptr<work_contract_group> create
        (
            configuration const &
        );

        ~work_contract_group() = default;

        work_contract create_contract
        (
            work_contract::contract_configuration_type const &
        );

        void service_contracts();

        bool get_service_requested() const;

        std::size_t get_capacity() const;

        bool update_contract
        (
            work_contract &,
            work_contract::contract_configuration_type const &
        );

    private:

        using element_type = std::uint64_t;
        static auto constexpr bits_per_byte = 8;
        static auto constexpr bits_per_element_type = (sizeof(element_type) * bits_per_byte);
        static auto constexpr bits_per_contract = 4; // must be power of two
        static auto constexpr contracts_per_element_type = (bits_per_element_type / bits_per_contract);


        struct contract_info
        {
            std::function<void()>               contractHandler_;
            std::function<void()>               endContractHandler_;
        };

        struct shared_state
        {
            struct configuration
            {
                std::size_t                 capacity_{default_capacity};
            };

            shared_state
            (
                configuration config
            ):
                contractStateFlags_((config.capacity_ + contracts_per_element_type - 1) / contracts_per_element_type),
                contracts_(config.capacity_)
            {
            }
            std::vector<std::atomic<element_type>>  contractStateFlags_;
            std::vector<contract_info>              contracts_;
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
            configuration const &
        );

        void on_end_contract
        (
            std::uint64_t
        );

        std::shared_ptr<shared_state>       sharedState_;

        std::size_t                         capacity_;
    }; // class work_contract_group

} // namespace maniscalco::system

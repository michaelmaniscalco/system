#pragma once

#include "./work_contract.h"

#include <cstdint>
#include <functional>
#include <atomic>


namespace maniscalco::system
{

    class alertable_work_contract
    {
    public:

        alertable_work_contract() = default;

        alertable_work_contract
        (
            work_contract,
            std::function<void()> 
        );

        alertable_work_contract(alertable_work_contract &&) = default;
        alertable_work_contract & operator = (alertable_work_contract &&) = default;

        void invoke();

        void surrender();

        void operator()();
        
        bool is_valid() const;
        
        bool update
        (
            work_contract::contract_configuration_type const &
        );

    private:

        work_contract workContract_;

        std::function<void()> invokeHandler_;
    };

} // namespace maniscalco::system

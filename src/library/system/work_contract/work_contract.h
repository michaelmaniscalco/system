#pragma once

#include <cstdint>
#include <functional>
#include <atomic>


namespace maniscalco::system
{

    class work_contract_group;
    

    class work_contract final
    {
    public:
               
        using contract_handler = std::function<void()>;
        using end_contract_handler = std::function<void()>;

        struct contract_configuration_type
        {
            contract_handler        contractHandler_;
            end_contract_handler    endContractHandler_;
        };

        work_contract();
        
        work_contract
        (
            work_contract &&
        );
      
        work_contract & operator = 
        (
            work_contract &&
        );
                  
        ~work_contract();
        
        void operator()();

        void invoke();
        
        void surrender();
        
        bool is_valid() const;
        
        bool update
        (
            contract_configuration_type const &
        );
    
    private:
    
        friend class work_contract_group;

        struct configuration_type
        {
            std::uint64_t                   invokeFlags_;
            std::uint64_t                   surrenderFlags_;
            std::atomic<std::uint64_t> *    flags_;
            std::uint64_t                   index_{0};
        };

        work_contract
        (
            work_contract_group *,
            configuration_type
        );
    
        work_contract(work_contract const &) = delete;
        work_contract & operator = (work_contract const &) = delete;

        std::uint64_t                   index_{0};

        std::uint64_t                   invokeFlags_{0};
        
        std::uint64_t                   surrenderFlags_{0};

        std::atomic<std::uint64_t> *    flags_{nullptr};

        work_contract_group *           workContractGroup_{nullptr};
    };
    
} // namespace maniscalco::system

        
//===================================================================================================================== 
inline void maniscalco::system::work_contract::invoke
(
)
{
    *flags_ |= invokeFlags_;
}


//===================================================================================================================== 
inline void maniscalco::system::work_contract::operator()
(
)
{
    *flags_ |= invokeFlags_;
}

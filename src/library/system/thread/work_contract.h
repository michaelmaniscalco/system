#pragma once

#include <cstdint>
#include <functional>


namespace maniscalco::system
{

    class work_contract final
    {
    public:
    
        using contract_handler = std::function<void()>;
        using end_contract_handler = std::function<void()>;
        
        struct configuration_type
        {
            contract_handler        contractHandler_;
            end_contract_handler    endContractHandler_;
        };
        
        work_contract() = default;
        
        work_contract
        (
            work_contract &&
        );
      
        work_contract & operator = 
        (
            work_contract &&
        );
                  
        ~work_contract();
        
        void exercise();
        
    protected:
    
    private:
    
        friend class work_contract_group;
    
        work_contract
        (
            configuration_type
        );
    
        work_contract(work_contract const &) = delete;
        work_contract & operator = (work_contract const &) = delete;
    
        end_contract_handler    endContractHandler_;
        
        contract_handler        contractHandler_;
    };
    
} // namespace maniscalco::system

        
//===================================================================================================================== 
inline void maniscalco::system::work_contract::exercise
(
)
{
    contractHandler_();
}

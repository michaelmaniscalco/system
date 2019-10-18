#include "./work_contract.h"


//===================================================================================================================== 
maniscalco::system::work_contract::work_contract
(
    configuration_type configuration
):
    endContractHandler_(configuration.endContractHandler_),
    contractHandler_(configuration.contractHandler_)
{
}


//===================================================================================================================== 
maniscalco::system::work_contract::work_contract
(
    work_contract && other
):
    endContractHandler_(std::move(other.endContractHandler_)),
    contractHandler_(std::move(other.contractHandler_))
{
    other.endContractHandler_ = nullptr;
    other.contractHandler_ = nullptr;
}

      
//===================================================================================================================== 
auto maniscalco::system::work_contract::operator = 
(
    work_contract && other
) -> work_contract &
{
    if (endContractHandler_)
        endContractHandler_();
    endContractHandler_ = std::move(other.endContractHandler_);
    other.endContractHandler_ = nullptr;
    contractHandler_ = std::move(other.contractHandler_);
    other.contractHandler_ = nullptr;
    return *this;
}
        
        
//===================================================================================================================== 
maniscalco::system::work_contract::~work_contract
(
)
{
    if (endContractHandler_)
        endContractHandler_();
}

        
//===================================================================================================================== 
void maniscalco::system::work_contract::exercise
(
)
{
    if (contractHandler_)
        contractHandler_();
}

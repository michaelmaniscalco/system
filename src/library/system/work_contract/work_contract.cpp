#include "./work_contract.h"

#include "./work_contract_group.h"


std::atomic<std::uint64_t> maniscalco::system::work_contract::dummyFlags_;


//===================================================================================================================== 
maniscalco::system::work_contract::work_contract
(
):
    flags_(&dummyFlags_)
{
}


//===================================================================================================================== 
maniscalco::system::work_contract::work_contract
(
    work_contract_group * workContractGroup,
    configuration_type configuration
):
    invokeFlags_(configuration.invokeFlags_),
    surrenderFlags_(configuration.surrenderFlags_),
    flags_(configuration.flags_),
    index_(configuration.index_),
    workContractGroup_(workContractGroup)
{
}


//===================================================================================================================== 
maniscalco::system::work_contract::work_contract
(
    work_contract && other
):
    invokeFlags_(other.invokeFlags_),
    surrenderFlags_(other.surrenderFlags_),
    flags_(other.flags_),
    index_(other.index_),
    workContractGroup_(other.workContractGroup_)
{
    other.flags_ = &dummyFlags_;
    other.invokeFlags_ = 0;
    other.surrenderFlags_ = 0;
    other.index_ = -1;
}

      
//===================================================================================================================== 
auto maniscalco::system::work_contract::operator = 
(
    work_contract && other
) -> work_contract &
{
    *flags_ |= surrenderFlags_;
    invokeFlags_ = other.invokeFlags_;
    surrenderFlags_ = other.surrenderFlags_;
    index_ = other.index_;
    flags_ = other.flags_;
    workContractGroup_ = other.workContractGroup_;
    other.flags_ = &dummyFlags_;
    other.index_ = -1;
    return *this;
}
        
        
//===================================================================================================================== 
maniscalco::system::work_contract::~work_contract
(
)
{
    *flags_ |= surrenderFlags_;
}


//=====================================================================================================================
void maniscalco::system::work_contract::surrender
(
)
{
    *this = std::move(work_contract());
}


//=============================================================================
bool maniscalco::system::work_contract::is_valid
(
) const
{
    return (flags_ != &dummyFlags_);
}


//=============================================================================
bool maniscalco::system::work_contract::update
(
    contract_configuration_type const & config
)
{
    return workContractGroup_->update_contract(*this, config);
}

#include "./work_contract.h"

namespace
{
    static std::atomic<std::uint64_t> dummy;
}


//===================================================================================================================== 
maniscalco::system::work_contract::work_contract
(
):
    flags_(&dummy)
{
}


//===================================================================================================================== 
maniscalco::system::work_contract::work_contract
(
    configuration_type configuration
):
    invokeFlags_(configuration.invokeFlags_),
    surrenderFlags_(configuration.surrenderFlags_),
    flags_(configuration.flags_),
    index_(configuration.index_)
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
    index_(other.index_)
{
    other.flags_ = &dummy;
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
    other.flags_ = &dummy;
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
    return (flags_ != &dummy);
}

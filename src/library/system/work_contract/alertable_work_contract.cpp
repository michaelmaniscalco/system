#include "./alertable_work_contract.h"


//=============================================================================
maniscalco::system::alertable_work_contract::alertable_work_contract
(
    work_contract workContract,
    std::function<void()> invokeHandler 
):
    workContract_(std::move(workContract)),
    invokeHandler_(invokeHandler)
{
}


//=============================================================================
void maniscalco::system::alertable_work_contract::invoke
(
)
{
    workContract_.invoke();
    invokeHandler_();
}


//=============================================================================
void maniscalco::system::alertable_work_contract::surrender
(
)
{
    workContract_.surrender();
    invokeHandler_();
}


//=============================================================================
void maniscalco::system::alertable_work_contract::operator()
(
)
{
    invoke();   
}


//=============================================================================
bool maniscalco::system::alertable_work_contract::is_valid
(
) const
{
    return workContract_.is_valid();
}


//=============================================================================
bool maniscalco::system::alertable_work_contract::update
(
    work_contract::contract_configuration_type const & config
)
{
    return workContract_.update(config);
}

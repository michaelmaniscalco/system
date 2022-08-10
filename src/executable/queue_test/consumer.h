#pragma once

#include <library/system.h>
#include "./queue.h"

#include <memory>


namespace maniscalco
{

    template <typename T>
    class consumer
    {
    public:

        using value_type = T;

        using receive_handler = std::function<void(consumer const &, T)>;

        struct configuration
        {
            receive_handler     receiveHandler_;
        };

        consumer
        (
            configuration const &,
            system::work_contract_group &
        );

        consumer(consumer const &) = delete;
        consumer & operator = (consumer const &) = delete;

        std::shared_ptr<queue<value_type>> get_queue() const;

    private:

        void receive_data();

        system::work_contract               workContract_;

        std::shared_ptr<queue<value_type>>  queue_;

        receive_handler                     receiveHandler_;

    }; // class consumer

} // namespace maniscalco


//=============================================================================
template <typename T>
maniscalco::consumer<T>::consumer
(
    configuration const & config,
    system::work_contract_group & workContractGroup
):
    receiveHandler_(config.receiveHandler_)
{
    auto workContract = workContractGroup.create_contract(
            {
                .contractHandler_ = [this](){this->receive_data();}
            });
    if (!workContract)
        throw std::runtime_error("failed to get work contract");
    workContract_ = std::move(*workContract);
    queue_ = std::make_shared<queue<value_type>>(
            queue<value_type>::configuration{
                .receiveHandler_ = [this]
                (
                    auto const &
                ) mutable
                {
                    this->workContract_.invoke();
                }
            });
}


//=============================================================================
template <typename T>
maniscalco::consumer<T>::consumer
(
    consumer && other
):
    workContract_(std::move(other.workContract_)), 
    queue_(std::move(other.queue_))
{
}


//=============================================================================
template <typename T>
auto maniscalco::consumer<T>::operator = 
(
    consumer && other
) -> consumer & 
{
    workContract_ = std::move(other.workContract_); 
    queue_ = std::move(other.queue_);
    return *this;
}


//=============================================================================
template <typename T>
auto maniscalco::consumer<T>::get_queue
(
) const -> std::shared_ptr<queue> 
{
    return queue_;
}


//=============================================================================
template <typename T>
void maniscalco::consumer<T>::receive_data
(
    // this is invoked by the work contract whenever the contract is invoked.
    // basically this is every time that the queue receives data
)
{
    // invoked by contract via queue
    receiveHandler_(*this, queue_->pop());
    std::cout << "receiver got value " << value << std::endl;
    if (!queue_->empty())
        workContract_.invoke(); // but wait! there's more! (trigger re-invocation of the contract if not empty)
}
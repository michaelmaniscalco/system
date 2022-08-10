#pragma once

#include "./queue.h"
#include "./consumer.h"

#include <memory>


namespace maniscalco
{

    template <typename T>
    class producer
    {
    public:

        using value_type = T;

        void connect_to
        (
            consumer<T> &
        );

        void produce
        (
            T
        );

    private:

        std::shared_ptr<queue<T>> queue_;

    }; // class producer

} // namespace maniscalco


//=============================================================================
template <typename T>
void maniscalco::producer<T>::connect_to
(
    consumer<T> & destination
)
{
    queue_ = destination.get_queue();
}


//=============================================================================
template <typename T>
void maniscalco::producer<T>::produce
(
    value_type value
)
{
    while (!queue_->push(value))
        ;
}

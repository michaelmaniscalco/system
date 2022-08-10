#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <atomic>


namespace maniscalco
{

    template <typename T>
    class queue
    {
    public:

        static auto constexpr default_capacity = 8192ull;

        using value_type = T;
        using receive_handler = std::function<void(queue const &)>;

        struct configuration
        {
            std::size_t capacity_{default_capacity};
            receive_handler receiveHandler_;
        };

        queue
        (
            configuration const &
        );

        bool push
        (
            value_type 
        );

        value_type pop();

        bool empty() const;

    private:

        receive_handler receiveHandler_;

        std::unique_ptr<value_type []>  queue_;

        std::size_t                     capacity_;

        std::atomic<std::size_t>        front_{0};

        std::atomic<std::size_t>        back_{0};
    };

} // namespace maniscalco


//=============================================================================
template <typename T>
maniscalco::queue<T>::queue
(
    configuration const & config
):
    receiveHandler_(config.receiveHandler_),
    queue_(new value_type [config.capacity_]),
    capacity_(config.capacity_)
{
}


//=============================================================================
template <typename T>
bool maniscalco::queue<T>::push
(
    value_type value 
)
{
    if (back_ >= (front_ + capacity_))
        return false;
    queue_[back_++ % capacity_] = value;
    receiveHandler_(*this);
    return true;
}


//=============================================================================
template <typename T>
auto maniscalco::queue<T>::pop
(
) -> value_type
{
    return queue_[front_++ % capacity_];
}


//=============================================================================
template <typename T>
bool maniscalco::queue<T>::empty
(
) const  
{
    return (front_ == back_);
}

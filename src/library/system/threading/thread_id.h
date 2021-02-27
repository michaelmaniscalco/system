#pragma once

#include <type_safe/strong_typedef.hpp>

#include <cstdint>


namespace maniscalco
{

    class thread_count : 
        public type_safe::strong_typedef<thread_count, std::size_t>,
        public type_safe::strong_typedef_op::equality_comparison<thread_count>,
        public type_safe::strong_typedef_op::relational_comparison<thread_count>,
        public type_safe::strong_typedef_op::increment<thread_count>
    {
    public:
        using strong_typedef::strong_typedef;
        auto get() const{return type_safe::get(*this);}
    };

} // namespace maniscalco

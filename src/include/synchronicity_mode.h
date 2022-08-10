#pragma once

#include <cstdint>


namespace maniscalco::system
{

    enum class synchronicity_mode : std::uint32_t
    {
        synchronous = 0,
        blocking = synchronous,
        sync = synchronous,
        asynchronous = 1,
        non_blocking = asynchronous,
        async = asynchronous
    };

} // namespace maniscalco

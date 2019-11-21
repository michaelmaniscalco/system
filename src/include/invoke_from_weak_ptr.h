#pragma once

#include <type_traits>
#include <memory>
#include <optional>


namespace maniscalco
{
    
    template <typename T, typename R, typename ... function_argument_types, typename ... argument_types>
    auto invoke_from_weak_ptr
    (
        R(T::*)(function_argument_types ...),
        std::weak_ptr<T> const &,
        argument_types && ... arguments
    ) -> typename std::enable_if<std::is_same<R, void>::value, std::optional<bool>>::type;
    
    
    template <typename T, typename R, typename ... function_argument_types, typename ... argument_types>
    auto invoke_from_weak_ptr
    (
        R(T::*)(function_argument_types ...),
        std::weak_ptr<T> const &,
        argument_types && ... arguments
    ) -> typename std::enable_if<!std::is_same<R, void>::value, std::optional<R>>::type;

} // namespace maniscalco


//==============================================================================
template <typename T, typename R, typename ... function_argument_types, typename ... argument_types>
auto maniscalco::invoke_from_weak_ptr
(
    R(T::*function)(function_argument_types ...),
    std::weak_ptr<T> const & wp,
    argument_types && ... arguments
) -> typename std::enable_if<std::is_same<R, void>::value, std::optional<bool>>::type
{
    if (auto sp = wp.lock())
    { 
        (sp.get()->*function)(std::forward<argument_types>(arguments) ...);
        return true;
    }
    return std::nullopt;
}


//==============================================================================
template <typename T, typename R, typename ... function_argument_types, typename ... argument_types>
auto maniscalco::invoke_from_weak_ptr
(
    R(T::*function)(function_argument_types ...),
    std::weak_ptr<T> const & wp,
    argument_types && ... arguments
) -> typename std::enable_if<!std::is_same<R, void>::value, std::optional<R>>::type
{
    if (auto sp = wp.lock()) 
        return (sp.get()->*function)(std::forward<argument_types>(arguments) ...);
    return std::nullopt;
}


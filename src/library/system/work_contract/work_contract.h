#pragma once

#include <atomic>
#include <cstdint>


namespace maniscalco::system
{

    class work_contract_group;


    class work_contract
    {
    public:

        using id_type = std::uint32_t;

        work_contract() = default;
        ~work_contract();

        work_contract(work_contract &&);
        work_contract & operator = (work_contract &&);

        work_contract(work_contract const &) = delete;
        work_contract & operator = (work_contract const &) = delete;

        void operator()();

        void invoke();

        void surrender();

        bool is_valid() const;

        operator bool() const;

        id_type get_id() const;

        bool update
        (
            std::function<void()>
        );

        bool update
        (
            std::function<void()>,
            std::function<void()>
        );

    private:

        friend class work_contract_group;

        work_contract
        (
            work_contract_group *, 
            id_type
        );

        work_contract_group *   owner_{};

        id_type                 id_{};

    }; // class work_contract

} // namespace maniscalco::system


#include "./work_contract_group.h"


//=============================================================================
inline maniscalco::system::work_contract::work_contract
(
    work_contract_group * owner, 
    id_type id
):
    owner_(owner),
    id_(id)
{
}


//=============================================================================
inline maniscalco::system::work_contract::work_contract
(
    work_contract && other
):
    owner_(other.owner_),
    id_(other.id_)
{
    other.owner_ = {};
    other.id_ = {};
}

    
//=============================================================================
inline auto maniscalco::system::work_contract::operator =
(
    work_contract && other
) -> work_contract &
{
    surrender();

    owner_ = other.owner_;
    id_ = other.id_;
    
    other.owner_ = {};
    other.id_ = {};
    return *this;
}


//=============================================================================
inline maniscalco::system::work_contract::~work_contract
(
)
{
    surrender();
}


//=============================================================================
inline auto maniscalco::system::work_contract::get_id
(
) const -> id_type
{
    return id_;
}


//=============================================================================
inline void maniscalco::system::work_contract::invoke
(
)
{
    owner_->invoke(*this);
}


//=============================================================================
inline void maniscalco::system::work_contract::operator()
(
)
{
    invoke();
}


//=============================================================================
inline void maniscalco::system::work_contract::surrender
(
)
{
    if (owner_)
        std::exchange(owner_, nullptr)->surrender(*this);
}


//=============================================================================
inline bool maniscalco::system::work_contract::is_valid
(
) const
{
    return (owner_ != nullptr);
}


//=============================================================================
inline maniscalco::system::work_contract::operator bool
(
) const
{
    return is_valid();
}


//=============================================================================
inline bool maniscalco::system::work_contract::update
(
    std::function<void()> function
)
{
    if (owner_)
        return owner_->update(*this, function);
    return false;
}


//=============================================================================
inline bool maniscalco::system::work_contract::update
(
    std::function<void()> function,
    std::function<void()> surrender
)
{
    if (owner_)
        return owner_->update(*this, function, surrender);
    return false;
}

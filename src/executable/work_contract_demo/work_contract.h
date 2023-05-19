#pragma once

#include <atomic>
#include <cstdint>


class work_contract_group;


class work_contract
{
public:

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

    std::size_t get_id() const;

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
        std::size_t
    );

    work_contract_group *   owner_{};

    std::size_t             id_{};
};


#include "./work_contract_group.h"


//=============================================================================
inline work_contract::work_contract
(
    work_contract_group * owner, 
    std::size_t id
):
    owner_(owner),
    id_(id)
{
}


//=============================================================================
inline work_contract::work_contract
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
inline auto work_contract::operator =
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
inline work_contract::~work_contract
(
)
{
    surrender();
}


//=============================================================================
inline std::size_t work_contract::get_id
(
) const
{
    return id_;
}


//=============================================================================
inline void work_contract::invoke
(
)
{
    owner_->invoke(*this);
}


//=============================================================================
inline void work_contract::operator()
(
)
{
    invoke();
}


//=============================================================================
inline void work_contract::surrender
(
)
{
    if (owner_)
        std::exchange(owner_, nullptr)->surrender(*this);
}


//=============================================================================
inline bool work_contract::is_valid
(
) const
{
    return (owner_ != nullptr);
}


//=============================================================================
inline work_contract::operator bool
(
) const
{
    return is_valid();
}


//=============================================================================
inline bool work_contract::update
(
    std::function<void()> function
)
{
    if (owner_)
        return owner_->update(*this, function);
    return false;
}


//=============================================================================
inline bool work_contract::update
(
    std::function<void()> function,
    std::function<void()> surrender
)
{
    if (owner_)
        return owner_->update(*this, function, surrender);
    return false;
}

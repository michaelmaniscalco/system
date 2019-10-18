#include "./thread_pool.h"


//=====================================================================================================================
maniscalco::thread_pool::thread_pool
(
    std::size_t capacity,
    std::function<void()> threadFunction
):
    threads_(),
    terminateFlag_(std::make_shared<bool volatile>(false))
{
    threads_.resize(capacity);
    for (auto & thread : threads_)
    {
        thread = std::thread([threadFunction]
                (
                    std::shared_ptr<bool volatile const> terminateFlag
                )
                {
                    while (!*terminateFlag)
                        threadFunction();
                },
                terminateFlag_);
    }
}
    

//=====================================================================================================================
maniscalco::thread_pool::~thread_pool
(
)
{
    *terminateFlag_ = true;
    for (auto & thread : threads_)
        thread.join();
}

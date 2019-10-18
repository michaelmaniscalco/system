#include "./thread_pool.h"
#include <library/system.h>


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
    auto cpuId = 0;
    for (auto & thread : threads_)
    {
        thread = std::thread([threadFunction, cpuId]
                (
                    std::shared_ptr<bool volatile const> terminateFlag
                )
                {
                    maniscalco::system::set_cpu_affinity(cpuId);
                    while (!*terminateFlag)
                        threadFunction();
                },
                terminateFlag_);
        cpuId += 2;
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

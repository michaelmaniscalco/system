#include "./thread_pool.h"

#include <range/v3/view/enumerate.hpp>

#include <atomic>

//=============================================================================
maniscalco::system::thread_pool::thread_pool
(
    configuration const & config
):
    threads_(),
    terminateFlag_(false)
{
    threads_.resize(config.threads_.size());

    for (auto && [index, thread] : ranges::views::enumerate(threads_))
    {
        thread = std::jthread([config = config.threads_[index]]
                (
                    bool volatile const & terminateFlag
                )
                {
                    try
                    {
                        if (config.initializeHandler_)
                            config.initializeHandler_();
                        while (!terminateFlag)
                            config.function_();
                        if (config.terminateHandler_)
                            config.terminateHandler_();
                    }
                    catch (std::exception const & exception)
                    {
                        if (config.exceptionHandler_)
                            config.exceptionHandler_(exception);
                    }
                },
                std::cref(terminateFlag_));
    }
}
    

//=============================================================================
maniscalco::system::thread_pool::~thread_pool
(
)
{
    terminateFlag_ = true;
    for (auto & thread : threads_)
        if (thread.joinable())
            thread.join();
}


//=============================================================================
void maniscalco::system::thread_pool::stop
(
    // issue terminate to all worker threads
)
{
    stop(synchronicity_mode::async);
}


//=============================================================================
void maniscalco::system::thread_pool::stop
(
    // issue terminate to all worker threads. 
    // waits for all threads to terminate if waitForTerminationComplete is true
    synchronicity_mode stopMode
)
{
    terminateFlag_ = true;
    if (stopMode == synchronicity_mode::blocking)
        for (auto & thread : threads_)
            if (thread.joinable())
                thread.join();
}

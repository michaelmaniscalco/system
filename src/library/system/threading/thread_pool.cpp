#include "./thread_pool.h"

#include <range/v3/view/enumerate.hpp>


//=============================================================================
maniscalco::system::thread_pool::thread_pool
(
    configuration const & config
):
    threads_(config.threads_.size())
{
    for (auto && [index, thread] : ranges::views::enumerate(threads_))
    {
        thread = std::jthread([config = config.threads_[index]]
                (
                    std::stop_token stopToken
                )
                {
                    try
                    {
                        if (config.initializeHandler_)
                            config.initializeHandler_();
                        while (!stopToken.stop_requested())
                            config.function_(stopToken);
                        if (config.terminateHandler_)
                            config.terminateHandler_();
                    }
                    catch (...)
                    {
                        auto currentException = std::current_exception();
                        if (config.exceptionHandler_)
                            config.exceptionHandler_(currentException);
                        else
                            std::rethrow_exception(currentException);
                    }
                });
    }
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
    for (auto & thread : threads_)
        thread.request_stop();

    if (stopMode == synchronicity_mode::blocking)
        for (auto & thread : threads_)
            if (thread.joinable())
                thread.join();
}

#include "./thread_pool.h"


//=============================================================================
maniscalco::system::thread_pool::thread_pool
(
    configuration_type const & configuration
):
    threads_(),
    terminateFlag_(false)
{
    threads_.resize(configuration.threadCount_);
    for (auto & thread : threads_)
    {
        thread = std::thread([threadFunction = configuration.workerThreadFunction_]
                (
                    bool volatile const & terminateFlag
                )
                {
                    while (!terminateFlag)
                        threadFunction();
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
    stop(stop_mode::async);
}


//=============================================================================
void maniscalco::system::thread_pool::stop
(
    // issue terminate to all worker threads. 
    // waits for all threads to terminate if waitForTerminationComplete is true
    stop_mode stopMode
)
{
    terminateFlag_ = true;
    if (stopMode == stop_mode::blocking)
        for (auto & thread : threads_)
            if (thread.joinable())
                thread.join();
}

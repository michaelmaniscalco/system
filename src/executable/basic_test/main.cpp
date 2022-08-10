#include <cstddef>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <library/system.h>

#include <range/v3/view/enumerate.hpp>


int main
(
    int, 
    char const **
)
{
    using namespace maniscalco::system;

    // create a work_contract_group - very simple
    auto workContractGroup = work_contract_group::create({});

    std::condition_variable conditionVariable;
    std::mutex mutex;
    std::size_t invokationCounter{0};

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    static auto constexpr thread_count = 8;
    std::vector<thread_pool::thread_configuration> threads(thread_count);
    for (auto & thread : threads)
        thread.function_ = [&, previousInvokationCounter = 0]() mutable
                        {
                                std::unique_lock uniqueLock(mutex);
                                if (previousInvokationCounter != invokationCounter)
                                {
                                    previousInvokationCounter = invokationCounter;
                                    uniqueLock.unlock();
                                    workContractGroup->service_contracts();
                                }
                                else
                                {
                                    if (conditionVariable.wait_for(uniqueLock, std::chrono::milliseconds(5), [&](){return (previousInvokationCounter != invokationCounter);}))
                                    {
                                        uniqueLock.unlock();
                                        workContractGroup->service_contracts(); 
                                    }
                                }
                        };

    thread_pool workerThreadPool({.threads_ = threads});

    // create work_contracts.  we will use a custom wrapper class around work contract
    // so that we can use condition_variable to wake up threads from the thread pool to process
    // the work rather than having the threads spin (consuming all CPU) while waiting for 
    // work contracts.
    static auto constexpr num_work_contracts = 10;
    std::vector<alertable_work_contract> workContracts(num_work_contracts);
    std::vector<std::atomic<bool>> workCompleted(workContracts.size());
    std::vector<std::atomic<bool>> contractExpired(workContracts.size());
    for (auto && [index, workContract] : ranges::views::enumerate(workContracts))
            workContract = {workContractGroup->create_contract(
            {
                .contractHandler_ = [w = &workCompleted[index]]() mutable
                {
                    *w = true;
                },
                .endContractHandler_ = [w = &contractExpired[index]]() mutable
                {
                    *w = true;
                }
            }), 
            [&]()
            {
                {
                    std::unique_lock uniqueLock(mutex);
                    ++invokationCounter;
                }
                conditionVariable.notify_one();
            }};

    // invoke the contracts - one of the worker threads will then service the contract asyncrhonously 
    for (auto & workContract : workContracts)
        workContract.invoke();

    // wait for all work contracts to be completed
    for (auto const & w : workCompleted)
        while (!w)
            ;
    std::cout << "all tasks completed\n";

    // surrender the contracts - also processed asynchronously by the worker threads
    for (auto & workContract : workContracts)
        workContract.surrender();

    // wait for all contracts to be surrendered
    for (auto const & w : contractExpired)
        while (!w)
            ;
    std::cout << "all tasks expired\n";

    workerThreadPool.stop(synchronicity_mode::blocking);

    return 0;
}

#include <cstddef>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>


#include <library/system.h>


int main
(
    int, 
    char const **
)
{
    std::atomic<std::size_t> workCount{0};
    // create a work_contract_group - very simple
    auto workContractGroup = maniscalco::system::work_contract_group::create([&](){++workCount;});

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    maniscalco::system::thread_pool workerThreadPool(   
            {
                .threadCount_ = 4,
                .workerThreadFunction_ = [&]()
                        {
                            // wait until the there is work to do rather than spin.
                            auto expectedWorkCount = workCount.load();
                            if ((expectedWorkCount > 0) && (workCount.compare_exchange_weak(expectedWorkCount, expectedWorkCount - 1)))
                                workContractGroup->service_contracts();
                            else
                                std::this_thread::yield();
                        }
            });

    // create a work_contract
    bool volatile done = false;
    auto workContract = workContractGroup->create_contract(
            {
                .contractHandler_ = [&done](){std::cout << "Work contract exercised" << std::endl; done = true;},
                .endContractHandler_ = [](){std::cout << "Work contract expired" << std::endl;}
            });

    // invoke the contract - one of the worker threads will then service the contract asyncrhonously 
    workContract->invoke();
    while (!done)
        ;
    return 0;
}

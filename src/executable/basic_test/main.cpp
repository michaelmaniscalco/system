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
    std::condition_variable     conditionVariable;
    std::mutex                  mutex;
    
    // create a work_contract_group - very simple
    auto workContractGroup = maniscalco::system::work_contract_group::create(
            [&]()
            {
                // whenever a contract is excercised we use our condition variable to 'wake' a thread.
                conditionVariable.notify_one();
            });

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    maniscalco::system::thread_pool::configuration_type threadPoolConfiguration;
    threadPoolConfiguration.threadCount_ = 4;
    threadPoolConfiguration.workerThreadFunction_ = [&]()
            {
                // wait until the there is work to do rather than spin.
                std::unique_lock uniqueLock(mutex);
                std::chrono::microseconds waitTime(10);
                if (conditionVariable.wait_for(uniqueLock, waitTime, [&](){return workContractGroup->get_service_requested();}))
                    workContractGroup->service_contracts();
            };
    maniscalco::system::thread_pool workerThreadPool(threadPoolConfiguration);

    // create a work_contract
    maniscalco::system::work_contract_group::contract_configuration_type workContractConfiguration;
    workContractConfiguration.contractHandler_ = [&](){std::cout << "Work contract exercised" << std::endl;};
    workContractConfiguration.endContractHandler_ = [&](){std::cout << "Work contract expired" << std::endl;};
    auto workContract = workContractGroup->create_contract(workContractConfiguration);
    if (!workContract)
    {
        std::cout << "Failed to get contract" << std::endl;
        throw std::runtime_error("");
    }

    // invoke the contract - one of the worker threads will then service the contract asyncrhonously 
    workContract->exercise();

    // wait for a moment to ensure that the worker threads start before the main thread exits.
    // otherwise it is possible for the main thread to exit before the worker threads even get started
    // and we won't get our output to console.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return 0;
}

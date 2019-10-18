#include <cstddef>
#include <library/system.h>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>

#include "./thread_pool.h"



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
    static auto constexpr numWorkerThreads = 4;
    maniscalco::thread_pool workerThreadPool(numWorkerThreads, 
            [&]()
            {
                // wait until the there is work to do rather than spin.
                std::unique_lock uniqueLock(mutex);
                std::chrono::microseconds waitTime(10);
                if (conditionVariable.wait_for(uniqueLock, waitTime, [&](){return workContractGroup->get_service_requested();}))
                    workContractGroup->service_contracts();
            });

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

    // invoke the contract
    workContract->exercise();
    
    return 0;
}

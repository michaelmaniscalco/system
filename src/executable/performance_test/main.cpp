#include <cstddef>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cstdint>

#include <library/system.h>


//#define PERIODIC_RENEW_CONTRACT // tests thread safety for creating/destroying contracts during testing
//#define SLEEP_WHILE_NO_WORK // causes threads to sleep while no work is present


namespace
{
    static auto constexpr test_duration = std::chrono::milliseconds(1000);
    static auto constexpr num_worker_threads = 2;
    static auto constexpr num_producer_threads = 2;
    std::size_t constexpr loop_count = 16; 
    
    #ifdef SLEEP_WHILE_NO_WORK
        std::condition_variable     _conditionVariable;
        std::mutex                  _mutex;
        auto _workContractGroup = maniscalco::system::work_contract_group::create
        (
            []()
            {
                _conditionVariable.notify_one();
            }
        );
    #else
        auto _workContractGroup = maniscalco::system::work_contract_group::create
        (
            nullptr
        );
    #endif
}


//======================================================================================================================
void producer_thread_function
(
    std::atomic<bool> & startFlag,
    std::atomic<bool> const & stopFlag,
    std::atomic<std::size_t> & activeCount,
    std::size_t volatile & taskCount,
    std::size_t cpuId
)
{
    maniscalco::system::set_cpu_affinity(cpuId);

    maniscalco::system::work_contract_group::contract_configuration_type workContract;
    workContract.contractHandler_ = [&](){++taskCount;};
    workContract.endContractHandler_ = nullptr;
    auto contract = _workContractGroup->create_contract(workContract);
    if (!contract)
    {
        std::cout << "Failed to get contract to thread pool contract group" << std::endl;
        throw std::runtime_error("");
    }

    // wait until all producer threads are ready before we let any begin the test
    if (++activeCount == num_producer_threads)
        startFlag = true;
    while (!startFlag)
        ;

    while (!stopFlag)
    {
        auto nextTaskCount = (taskCount + 1);
        contract->exercise();
        while ((taskCount != nextTaskCount) && (!stopFlag))
            ;
        #ifdef PERIODIC_RENEW_CONTRACT
            if ((taskCount % 1000) == 0)
            {
                contract = _workContractGroup->create_contract(workContract);
                if (!contract)
                {
                    std::cout << "Failed to get contract from work contract group" << std::endl;
                    throw std::runtime_error("");
                }
            }
        #endif
    }
}


//======================================================================================================================
std::int32_t main
(
    std::int32_t,
    char const **
)
{
    // for testing purposes we get this main thread off of the cores we will be using for timing tests
    maniscalco::system::set_cpu_affinity(std::thread::hardware_concurrency() - 1);

    // create really simple thread pool and basic worker thread function
    // which services are work contract group.
    maniscalco::system::thread_pool::configuration_type threadPoolConfiguration;
    threadPoolConfiguration.threadCount_ = num_worker_threads;
    threadPoolConfiguration.workerThreadFunction_ = []()
            {
                #ifdef SLEEP_WHILE_NO_WORK
                    std::unique_lock uniqueLock(_mutex);
                    if (_conditionVariable.wait_for(uniqueLock, std::chrono::microseconds(10), [](){return _workContractGroup->get_service_requested();}))
                        _workContractGroup->service_contracts();
                #else
                    _workContractGroup->service_contracts();
                #endif
            };
    maniscalco::system::thread_pool workerThreadPool(threadPoolConfiguration);

    // repeat test 'loop_count' times       
    for (std::size_t k = 0; k < loop_count; ++k)
    {
        std::cout << "test iteration " << (k + 1) << " of " << loop_count << ": " << std::flush;
        std::atomic<bool> stopFlag = false;
        std::atomic<bool> producerThreadsReadyFlag = false;
        std::atomic<std::size_t> activeProducerThreadCount(0);
        std::thread producer_threads[num_producer_threads];
        std::array<std::size_t volatile, num_producer_threads> taskCount;
        for (auto & e : taskCount)
            e = 0;

        // create producer threads
        std::size_t n = 0;
        std::size_t cpuId = num_worker_threads * 2;
        for (auto & producer_thread : producer_threads)
        {
            producer_thread = std::thread(&producer_thread_function, std::ref(producerThreadsReadyFlag), 
                    std::cref(stopFlag), std::ref(activeProducerThreadCount), std::ref(taskCount[n++]), cpuId);
            cpuId += 2;
        }

        // wait for all threads to be ready before we begin the test (for timing purposes).
        while (!producerThreadsReadyFlag)
            ;
        auto startTime = std::chrono::system_clock::now();
        std::this_thread::sleep_for(test_duration);

        // terminate threads
        stopFlag = true;
        auto stopTime = std::chrono::system_clock::now();

        for (auto & producer_thread : producer_threads)
            producer_thread.join();
        
        std::size_t total = 0;
        for (auto & n : taskCount)
            total += n;
            
        auto elapsedTime = (stopTime - startTime);
        auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;
        std::cout << "Average tasks per thread/sec: " <<(int) ((total / num_producer_threads) / test_duration_in_sec) << std::endl;
    }

    workerThreadPool.stop();
    _workContractGroup.reset();

    return 0;
}


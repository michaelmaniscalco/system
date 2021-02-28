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

//#define SLEEP_WHILE_NO_WORK     // causes threads to sleep while no work is present (be a good citizen)
//#define USE_MINIMAL_WORK_TASK   // use the simplest work possible to measure the max throughput of the work_contract plumbing itself

namespace
{
    static auto constexpr test_duration = std::chrono::milliseconds(1000);
    static auto constexpr num_worker_threads = 6;
    static auto constexpr num_work_contracts = 128;
    std::size_t constexpr loop_count = 10; 
    
    #ifdef SLEEP_WHILE_NO_WORK
        std::atomic<std::size_t> _workCount{0};
        auto _workContractGroup = maniscalco::system::work_contract_group::create([](){++_workCount;});
    #else
        auto _workContractGroup = maniscalco::system::work_contract_group::create({.capacity_ = num_work_contracts});
    #endif
}


//======================================================================================================================
std::size_t producer_thread_function
(
)
{
    std::atomic<bool> stopFlag = false;
    std::size_t result = 0;

    try
    {
        // construct some work to do which has some heft to it
        #ifdef USE_MINIMAL_WORK_TASK
            auto workTask = [](){return 1;};
        #else
            auto workTask = []()
                {
                    auto total = 0;
                    for (auto i = 0; i < 20; ++i)
                    {
                        std::string s = "5677467676457";
                        s += std::to_string(i);
                        total += std::atoi(s.c_str());
                    }
                    return (total + (total == 0));
                };
        #endif

        // this thread will manage many work contracts
        struct work_contract_info
        {
            maniscalco::system::work_contract workContract_;
            std::size_t volatile count_{0};
            bool volatile stopped_{false};
        };

        std::array<work_contract_info, num_work_contracts> workContracts;
        std::array<std::size_t volatile, num_work_contracts> contractCount;  
        auto contractIndex = 0;
        for (auto & workContract : workContracts)
            workContract.workContract_ = std::move(*_workContractGroup->create_contract(
                {
                    .contractHandler_ = [&, workTask]()
                    {
                        static std::atomic<int> n{0};
                        thread_local static bool b = [&]()
                                {
                                    std::this_thread::sleep_for(std::chrono::microseconds(n++));
                                    return true;
                                }();
                        
                        if (stopFlag)
                        {
                            // once the global stop flag is set - stopping the test - we exit this work contract
                            workContract.workContract_.surrender();
                            workContract.stopped_ = true; 
                        }
                        else
                        {
                            // we have a worker thread honoring our work contract. let's make it do some work for us.
                            workContract.count_ = workContract.count_ + (workTask() != 0);
                            // invoke the contract again so another thread will come back and do some more work
                            workContract.workContract_.invoke();
                        }
                    },
                }));

        // inovke each work contract to start them up - they will self re-invoke in the work contract for the purposes of this test.
        for (auto & workContract : workContracts)
            workContract.workContract_();

        // sleep while the work contracts keep being repeatedly invoked ...
        std::this_thread::sleep_for(test_duration);

        // terminate work contracts
        stopFlag = true;
        // wait for each contract to ack the stop and add up the total times which the contract was exercised
        for (auto & workContract : workContracts)
        {
            while (!workContract.stopped_)
                std::this_thread::yield();
            result += workContract.count_;
        }
    }
    catch (std::exception const & exception)
    {
        std::cerr << exception.what();
    }
    return result;
}


//======================================================================================================================
std::int32_t main
(
    std::int32_t,
    char const **
)
{
    // create really simple thread pool and basic worker thread function
    // which services are work contract group.
    maniscalco::system::thread_pool::configuration_type threadPoolConfiguration;
    threadPoolConfiguration.threadCount_ = num_worker_threads;
    threadPoolConfiguration.workerThreadFunction_ = []()
            {
             //   static bool once = [](){std::this_thread::sleep_for(std::chrono::seconds(50000)); return true;}();
                #ifdef SLEEP_WHILE_NO_WORK
                    auto spin = 8192;
                    auto expectedWorkCount = _workCount.load();
                    while ((spin--) && (expectedWorkCount > 0))
                    {
                        if (_workCount.compare_exchange_weak(expectedWorkCount, expectedWorkCount - 1))
                            _workContractGroup->service_contracts();
                    }
                    if (expectedWorkCount == 0)
                        std::this_thread::yield();
                #else
                    _workContractGroup->service_contracts();
                #endif
            };
    maniscalco::system::thread_pool workerThreadPool(threadPoolConfiguration);
    // repeat test 'loop_count' times       
    for (std::size_t k = 0; k < loop_count; ++k)
    {
        std::cout << "test iteration " << (k + 1) << " of " << loop_count << ": " << std::flush;
        auto startTime = std::chrono::system_clock::now();
        auto taskCount = producer_thread_function();
        auto stopTime = std::chrono::system_clock::now();
        auto elapsedTime = (stopTime - startTime);
        auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;

        std::cout << "Total tasks = " << taskCount << ", tasks per sec = " << (int)(taskCount / test_duration_in_sec) << 
                ", tasks per thread per sec = " << (int)((taskCount / test_duration_in_sec) / num_worker_threads) << std::endl;
    }

    bool const waitForTerminationComplete = true;
    workerThreadPool.stop(maniscalco::system::thread_pool::stop_mode::blocking);
    _workContractGroup.reset();

    return 0;
}


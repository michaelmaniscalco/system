#include <cstddef>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cstdint>

#include <range/v3/view/enumerate.hpp>

#include <library/system.h>

//#define SLEEP_WHILE_NO_WORK     // causes threads to sleep while no work is present (be a good citizen)
#define USE_MINIMAL_WORK_TASK   // use the simplest work possible to measure the max throughput of the work_contract plumbing itself


namespace 
{
    using namespace maniscalco::system;
    static auto constexpr test_duration = std::chrono::milliseconds(1000);
    static auto constexpr num_worker_threads = 6;
    static auto constexpr num_work_contracts = 128;
    std::size_t constexpr loop_count = 10; 
    
    // create a work_contract_group - very simple
    auto workContractGroup = work_contract_group::create({.capacity_ = num_work_contracts});

    std::condition_variable conditionVariable;
    std::mutex mutex;
    std::size_t invokationCounter{0};
}


//======================================================================================================================
std::size_t producer_thread_function
(
)
{
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
        #ifdef SLEEP_WHILE_NO_WORK
            std::array<maniscalco::system::alertable_work_contract, num_work_contracts> workContracts;
        #else
            std::array<maniscalco::system::work_contract, num_work_contracts> workContracts;
        #endif


        std::array<std::size_t volatile, num_work_contracts> contractCount{};  
        auto contractIndex = 0;
        for (auto && [index, workContract] : ranges::views::enumerate(workContracts))
            workContract = {workContractGroup->create_contract(
                {
                    .contractHandler_ = [&, workTask, index]()
                    {
                        // we have a worker thread honoring our work contract. let's make it do some work for us.
                        contractCount[index] = contractCount[index] + (workTask() != 0);
                        // invoke the contract again so another thread will come back and do some more work
                        workContracts[index].invoke();
                    },
                })
                #ifdef SLEEP_WHILE_NO_WORK
                    , [&]()
                    {
                        {
                            std::unique_lock uniqueLock(mutex);
                            ++invokationCounter;
                        }
                        conditionVariable.notify_one();
                    }};
                #else
                    };
                #endif
        // inovke each work contract to start them up - they will self re-invoke in the work contract for the purposes of this test.
        for (auto & workContract : workContracts)
            workContract.invoke();

        // sleep while the work contracts keep being repeatedly invoked ...
        std::this_thread::sleep_for(test_duration);

        // terminate work contracts
        for (auto & workContract : workContracts)
            workContract.surrender();

        // wait for each contract to ack the stop and add up the total times which the contract was exercised
        for (auto && [index, workContract] : ranges::views::enumerate(workContracts))
        {
            while (workContract.is_valid())
                std::this_thread::yield();
            result += contractCount[index];
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

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    std::vector<thread_pool::thread_configuration> threads(num_worker_threads);
    for (auto & thread : threads)
        thread.function_ = [&, previousInvokationCounter = 0]() mutable
                {
                    #ifdef SLEEP_WHILE_NO_WORK
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
                    #else
                        workContractGroup->service_contracts();
                    #endif
                };
    thread_pool workerThreadPool({.threads_ = threads});

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
    workerThreadPool.stop(maniscalco::system::synchronicity_mode::blocking);
    workContractGroup.reset();

    return 0;
}


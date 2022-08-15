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
#include <library/system/cpu_id.h>

static auto constexpr sleep_when_no_work = false;     // causes threads to sleep while no work is present (be a good citizen)
static auto constexpr use_minimal_work_task = true;  // use the simplest work possible to measure the max throughput of the work_contract plumbing itself


namespace 
{
    using namespace maniscalco::system;
    static auto constexpr test_duration = std::chrono::milliseconds(1000);
    static auto const num_worker_threads = ((std::thread::hardware_concurrency() + 1) / 2);
    static auto constexpr num_work_contracts = 128;
    std::size_t constexpr loop_count = 10; 
    
    // create a work_contract_group - very simple
    auto workContractGroup = work_contract_group::create({.capacity_ = num_work_contracts});

    std::condition_variable_any conditionVariable;
    std::mutex mutex;
    std::size_t invokationCounter{0};

    // create a work task that each work_contract will use for its primary
    // work.  If we are measuring just the work_contract plumbing (no actual work)
    // then the work task will simply return 1.  Otherwise, we assign some work
    // that has some measurable amount of CPU time associated with it instead.
    // This heavier work load is used to make the work take time and allows us
    // to measure how well work_contracts behave when there are concurrent contracts.
    auto workTask = []()
            {
                if constexpr (use_minimal_work_task)
                {
                    // minimal work. just return 1
                    return 1;
                }
                else
                {
                    // the work here is simply to ensure that it takes time.
                    // in the end we will simply return a non zero value
                    // but its written in such a way as to avoid having the 
                    // compiler figure this out and optimize away the work.
                    auto total = 0;
                    for (auto i = 0; i < 20; ++i)
                    {
                        std::string s = "5677467676457";
                        s += std::to_string(i);
                        total += std::atoi(s.c_str());
                    }
                    return (total + (total == 0));
                }
            };

    // this code will use a special wrapper class around standard 
    // work_contracts.  An alertable_work_contract takes a second
    // argument which is a function to execute each time the underlying
    // work_contract is invoked.  We use this function to notify worker
    // threads as to when there is a pending work_contract invokation.
    // if the test is compiled to let worker threads spin instead (not cpu friendly)
    // then this secondary function will be empty.
    auto workContractAlert = []()
            {
                if constexpr (sleep_when_no_work)
                {
                    // this test can work in one of two modes.
                    // if we choose to let worker threads sleep
                    // when there are no invoked work contracts
                    // then this code is used to delay the 
                    // worker threads without spinning.
                    {
                        std::unique_lock uniqueLock(mutex);
                        ++invokationCounter;
                    } 
                    conditionVariable.notify_one();
                }
            };


    // the worker threads need a primary function to repeatedly invoke.
    // if the test is compiled to allow the worker threads to sleep 
    // when there are no active work_contracts then they wait on a 
    // condition_variable which is triggered by the work_contracts as
    // they get invoked. 
    // otherwise the test is compiled to allow the worker threads to spin
    // constantly and therefore process work_contracts with lower latency.
    auto workerThreadFunction = [previousInvokationCounter = 0](std::stop_token const & stopToken) mutable
            {
                if constexpr (sleep_when_no_work)
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
                        conditionVariable.wait(uniqueLock, stopToken, [&](){return (previousInvokationCounter != invokationCounter);});
                        if (!stopToken.stop_requested())
                        {
                            uniqueLock.unlock();
                            workContractGroup->service_contracts(); 
                        }
                    }
                }
                else
                {
                    workContractGroup->service_contracts();
                }
            };
}


//======================================================================================================================
std::size_t run_work_contracts
(
)
{
    std::size_t result = 0;
    try
    {
        // create collection of work contracts
        std::array<maniscalco::system::alertable_work_contract, num_work_contracts> workContracts;
        std::array<std::size_t, num_work_contracts> contractCount{};  
        auto contractIndex = 0;
        for (auto && [index, workContract] : ranges::views::enumerate(workContracts))
            workContract = {workContractGroup->create_contract(
                {
                    .contractHandler_ = [&, i = index]() // main work for each work contract
                    {
                        contractCount[i] = contractCount[i] + (workTask() != 0); // do some work
                        workContracts[i].invoke(); // re-invoke the same contract (loop until test is terminated)
                    },
                }), 
                workContractAlert // the function to trigger with each work contract invokation
                };
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
        std::cerr << exception.what() << "\n";
    }
    return result;
}


int main
(
    int, 
    char const **
)
{
    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    std::vector<thread_pool::thread_configuration> threads(num_worker_threads);
    for (auto & thread : threads)
    {
        thread.initializeHandler_ = []{static std::atomic<cpu_id> cpuId; set_cpu_affinity(cpuId++);};
        thread.function_ = workerThreadFunction;
    }
    thread_pool workerThreadPool({.threads_ = threads});

    // repeat test 'loop_count' times       
    for (std::size_t k = 0; k < loop_count; ++k)
    {
        std::cout << "test iteration " << (k + 1) << " of " << loop_count << ": " << std::flush;
        auto startTime = std::chrono::system_clock::now();
        auto taskCount = run_work_contracts();
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


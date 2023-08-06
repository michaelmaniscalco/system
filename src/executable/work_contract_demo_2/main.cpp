#include <cstddef>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <vector>
#include <thread>
#include <queue>

#include <library/system.h>
#include <fmt/format.h>


using work_contract_group_type = maniscalco::system::waitable_work_contract_group;
using work_contract_type = maniscalco::system::work_contract<work_contract_group_type::mode>;


//=============================================================================
void example
(
)
{
    static auto constexpr num_messages = 128;

    // create work contract group
    static auto constexpr max_number_of_contracts = 32;
    work_contract_group_type workContractGroup(max_number_of_contracts);
    work_contract_type workContract;

    // create worker threads to service work contracts asynchronously
    auto max_number_of_worker_threads = std::thread::hardware_concurrency() / 2;
    std::vector<maniscalco::system::thread_pool::thread_configuration> threadConfigurations(max_number_of_worker_threads);
    for (auto & threadConfiguration : threadConfigurations)
        threadConfiguration.function_ = [&](auto const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_contracts();};
    maniscalco::system::thread_pool threadPool({.threads_ = threadConfigurations});

    // crude message stream ...
    std::mutex mutex;
    std::deque<std::string> deque;
    std::atomic<bool> done{false};

    auto receiveMessage = [&]()
            {
                std::string message;
                {
                    std::lock_guard lockGuard(mutex); 
                    if (deque.empty())
                        return; // no messages ....
                    // pop the next message from the queue
                    message = deque.front(); 
                    deque.pop_front(); 
                    // if there is more in the queue then re-invoke the contract
                    // so that another thread can service the remaining message(s)
                    if (!deque.empty())
                        workContract.invoke();
                }
                if (message.empty())
                {
                    std::cout << "got termination message\n";
                    done = true;
                }
                else
                    std::cout << "Got message: " << message << "\n";
            };

    auto sendMessage = [&](std::string message)
            {
                std::lock_guard lockGuard(mutex); 
                deque.push_back(message); // push message into the queue
                workContract.invoke(); // invoke the contract (which checks the queue for messages)
            };

    // create a contract to check for messages whenever the contract is invoked.
    workContract = workContractGroup.create_contract(receiveMessage);

    // produce a series of messages and place them into the queue
    for (auto i = 0; i < num_messages; ++i)
    {
        sendMessage(fmt::format("this is message #{}", i));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // send an empty message which indicates end of test messages
    sendMessage("");

    // wait for all the messages to be received
    while (!done)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    workContractGroup.stop();
    threadPool.stop();
}


//=============================================================================
int main
(
    int, 
    char const **
)
{    
    example();
    return 0;
}

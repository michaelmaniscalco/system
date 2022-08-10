#include <cstddef>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <mutex>
#include <queue>


#include <library/system.h>

namespace
{

    class data_link
    {
    public:

        using value_type = std::int32_t;
        using receive_handler = std::function<void(data_link const &)>;

        struct configuration
        {
            receive_handler receiveHandler_;
        };

        data_link
        (
            configuration const & config
        ):
            receiveHandler_(config.receiveHandler_)
        {
        }

        void push
        (
            value_type value 
        )
        {
            std::unique_lock uniqueLock(mutex_);
            queue_.push_back(value);
            receiveHandler_(*this);
        }

        value_type pop
        (
        )
        {
            std::unique_lock uniqueLock(mutex_);
            auto ret = queue_.front();
            queue_.pop_front();
            return ret;
        }

        bool empty
        (
        ) const  
        {
            std::unique_lock uniqueLock(mutex_);
            return queue_.empty();
        }

    private:

        receive_handler receiveHandler_;

        std::mutex mutable mutex_;

        std::deque<value_type> queue_;
    };


    class receiver
    {
    public:
        receiver
        (
            maniscalco::system::work_contract_group & workContractGroup
        )
        {
            // create a work contract which, when invoked, calls this->on_link_has_data
            auto workContract = workContractGroup.create_contract(
                {
                    .contractHandler_ = [this](){this->on_link_has_data();},
                    .endContractHandler_ = nullptr // not really interested in when the contract expires in this case
                });
            if (!workContract.is_valid())
                throw std::runtime_error("failed to get work contract");
            workContract_ = std::move(workContract); 
            // create a queue and move the work contract into it so that it will invoke the work contract
            // whenever it receives data.
            link_ = std::make_shared<data_link>(data_link::configuration{.receiveHandler_ = [this](auto const &) mutable{this->workContract_.invoke();}});
        }

        std::shared_ptr<data_link> get_link() const{return link_;}

    private:

        void on_link_has_data
        (
            // this is invoked by the work contract whenever the contract is invoked.
            // basically this is every time that the queue receives data
        )
        {
            // invoked by contract via queue
            auto value = link_->pop();
            std::cout << "receiver got value " << value << std::endl;
            if (!link_->empty())
                workContract_.invoke(); // but wait! there's more! (trigger re-invocation of the contract if not empty)
        }


        maniscalco::system::work_contract workContract_;

        std::shared_ptr<data_link> link_;
    };


    class transmitter
    {
    public:

        void connect_to
        (
            receiver & destination
        )
        {
            link_ = destination.get_link();
        }

        void send
        (
            std::int32_t value
        )
        {
            link_->push(value);
        }

    protected:

        std::shared_ptr<data_link> link_;
    };
}


int main
(
    int, 
    char const **
)
{
    using namespace maniscalco::system;

    // create a work_contract_group - very simple
    auto workContractGroup = maniscalco::system::work_contract_group::create({});
    static auto constexpr num_worker_threads = 4;
    
   // create a worker thread pool and direct the threads to service the work contract group - also very simple
    std::vector<thread_pool::thread_configuration> threads(num_worker_threads);
    for (auto & thread : threads)
        thread.function_ = [&]()
                {
                    workContractGroup->service_contracts();
                };
    thread_pool workerThreadPool({.threads_ = threads});

    // create a message receiver
    receiver myReceiver(*workContractGroup);

    // create a message transmitter and connect it to the receiver
    transmitter myTransmitter;
    myTransmitter.connect_to(myReceiver);

    // send a series of messages across the connection
    for (auto i = 0; i < 100; ++i)
        myTransmitter.send(i);
        
    // this is just a demo ... sleep until all messages are cross the link - rather than writing code to safely exit
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}

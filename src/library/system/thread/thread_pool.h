#pragma once

#include <thread>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>


namespace maniscalco::system 
{
    
    class thread_pool
    {
    public:
    
        using thread_pool_ready_handler = std::function<void()>;
        using worker_thread_function = std::function<void()>;
        
        struct configuration_type
        {
            std::size_t                 threadCount_;
            worker_thread_function      workerThreadFunction_;
        };

        thread_pool
        (
            configuration_type const &
        );
        
        ~thread_pool();

        void stop();

        void stop(bool);

    private:
    
        std::vector<std::thread>    threads_;

        bool volatile               terminateFlag_;
    };
    
} // namespace maniscalco::system 

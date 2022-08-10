#pragma once

#include "./thread_id.h"

#include <include/synchronicity_mode.h>

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

        struct thread_configuration
        {
            std::function<void()> initializeHandler_;
            std::function<void()> terminateHandler_;
            std::function<void(std::exception const &)> exceptionHandler_; 
            std::function<void()> function_;          
        };

        struct configuration
        {
            std::vector<thread_configuration>   threads_;
        };

        thread_pool
        (
            configuration const &
        );
        
        ~thread_pool();

        void stop();

        void stop(synchronicity_mode);

    private:
    
        std::vector<std::jthread>   threads_;

        bool volatile               terminateFlag_;
    };
    
} // namespace maniscalco::system 

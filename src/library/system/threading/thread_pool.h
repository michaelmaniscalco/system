#pragma once

#include <include/synchronicity_mode.h>

#include <exception>
#include <stdexcept>
#include <thread>
#include <vector>
#include <cstdint>
#include <functional>


namespace maniscalco::system 
{
    
    class thread_pool
    {
    public:

        struct thread_configuration
        {
            std::function<void()> initializeHandler_;
            std::function<void()> terminateHandler_;
            std::function<void(std::exception_ptr)> exceptionHandler_; 
            std::function<void(std::stop_token const &)> function_;          
        };

        struct configuration
        {
            std::vector<thread_configuration>   threads_;
        };

        thread_pool
        (
            configuration const &
        );

        void stop();

        void stop(synchronicity_mode);

    private:
    
        std::vector<std::jthread>   threads_;
    };
    
} // namespace maniscalco::system 

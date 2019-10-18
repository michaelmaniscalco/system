#pragma once

#include <thread>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>


namespace maniscalco
{
    
    class thread_pool
    {
    public:
    
        thread_pool
        (
            std::size_t,
            std::function<void()>
        );
        
        ~thread_pool();

    private:
    
        std::vector<std::thread>        threads_;

        std::shared_ptr<bool volatile>  terminateFlag_;
    };
    
} // namespace maniscalco

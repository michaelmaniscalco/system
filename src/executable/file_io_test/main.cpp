#include <condition_variable>
#include <cstddef>
#include <experimental/filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include <include/invoke_from_weak_ptr.h>
#include <library/system.h>



namespace
{

    // simple class that reads file in blocks and counts number of times that the target char appears within the file.
    // uses work_contract to do the work in asynchronous tasks.
    class file_char_counter : 
            public std::enable_shared_from_this<file_char_counter>
    {
    public:

        using begin_handler = std::function<void(file_char_counter const &)>;
        using end_handler = std::function<void(file_char_counter const &)>;

        struct configuration_type
        {
            std::size_t                         bufferSize_{1024};
            std::experimental::filesystem::path filePath_;
            char                                target_;

            begin_handler                       beginHandler_;
            end_handler                         endHandler_;
        };

        static std::shared_ptr<file_char_counter> create
        (
            maniscalco::system::work_contract_group & workContractGroup,
            configuration_type const & configuration
        )
        {
            auto fileProcessor = std::shared_ptr<file_char_counter>(new file_char_counter(configuration));
            if (!fileProcessor->initialize(workContractGroup, configuration))
                fileProcessor.reset();
            return fileProcessor;
        }

        char get_target() const{return target_;}

        std::size_t get_count() const{return matchCount_;}

        std::string const & get_file_path() const{ return filePath_;}

    protected:

    private:


        file_char_counter
        (
            configuration_type const & configuration
        ):
            beginHandler_(configuration.beginHandler_),
            endHandler_(configuration.endHandler_),
            workContract_(),
            inputBuffer_(configuration.bufferSize_),
            inputFileStream_(),
            matchCount_(0),
            target_(configuration.target_),
            filePath_(configuration.filePath_)
        {
        }


        bool initialize
        (
            maniscalco::system::work_contract_group & workContractGroup,
            configuration_type const & configuration            
        )
        {
            // open the input file
            inputFileStream_.open(configuration.filePath_, std::ios_base::in | std::ios_base::binary);
            if (!inputFileStream_.is_open())
            {
                std::cerr << "failed to open file \"" << configuration.filePath_ << "\"" << std::endl;
                return false;
            }
            // create a work contract
            maniscalco::system::work_contract_group::contract_configuration_type workContractConfiguration;
            workContractConfiguration.contractHandler_ = [wpThis = this->weak_from_this()] // invoke work contract
                        (
                        )
                        {
                            maniscalco::invoke_from_weak_ptr(&file_char_counter::process_data, wpThis);
                        };
            auto workContract = workContractGroup.create_contract(workContractConfiguration);
            if (!workContract)
            {
                std::cerr << "Failed to create work contract" << std::endl;
                return false;
            }
            workContract_ = std::move(*workContract);
            workContract_.exercise();
            return true;
        }


        void process_data
        (
            // invoked via work contract
        )
        {
            if (beginHandler_)
            {
                beginHandler_(*this);
                beginHandler_ = nullptr;
            }
            // load data
            inputFileStream_.read(inputBuffer_.data(), inputBuffer_.capacity());
            inputBuffer_.resize(inputFileStream_.gcount());
            // process data
            for (auto const & c : inputBuffer_)
                matchCount_ += (c == target_);
            
            if (!inputFileStream_.eof())
            {
                workContract_.exercise(); // more to do so come back again!
            }
            else
            {
                // no more to do
                if (endHandler_)
                {
                    endHandler_(*this);
                    endHandler_ = nullptr;
                }
            }
        }

        begin_handler                           beginHandler_;

        end_handler                             endHandler_;

        maniscalco::system::work_contract       workContract_;

        std::vector<char>                       inputBuffer_;

        std::ifstream                           inputFileStream_;

        std::size_t                             matchCount_;

        char const                              target_;

        std::string const                       filePath_;

    }; // class file_char_counter


    //=========================================================================
    auto load_file_paths
    (
        // quick and dirty function for recursively collecting file paths from specified directory
        std::experimental::filesystem::path directoryPath
    ) -> std::vector<std::experimental::filesystem::path>
    {
        std::vector<std::experimental::filesystem::path> results;
        for (auto const & filePath : std::experimental::filesystem::directory_iterator(directoryPath))
        {
            if (std::experimental::filesystem::is_directory(filePath))
            {
                auto subResults = load_file_paths(filePath);
                results.insert(results.end(), subResults.begin(), subResults.end());
            }
            else
            {
                results.push_back(filePath);
            }
        }
        return results;
    }


    //==========================================================
    // NOTE:
    // the following globals are here to remove the 'boilerplate' setup
    // of work contracts from the main code with the intention of highlighting
    // how little code is needed to acheive asynchronous tasks based work.
    // these globals are not intended to suggest that this is the best way to 
    // write professional level code.  (^:
    //==========================================================

    std::condition_variable     conditionVariable;
    std::mutex                  mutex;
    
    // create a work_contract_group - very simple
    auto workContractGroup = maniscalco::system::work_contract_group::create(
            []()
            {
                // whenever a contract is excercised we use our condition variable to 'wake' a thread.
                conditionVariable.notify_one();
            });

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    auto num_threads = std::thread::hardware_concurrency();
    maniscalco::system::thread_pool workerThreadPool(
            {
                num_threads,
                []()
                {
                    // wait until the there is work to do rather than spin.
                    std::unique_lock uniqueLock(mutex);
                    std::chrono::microseconds waitTime(10);
                    if (conditionVariable.wait_for(uniqueLock, waitTime, [&](){return workContractGroup->get_service_requested();}))
                        workContractGroup->service_contracts();
                }
            });

} // anonymous namespace


int main
(
    int, 
    char const ** argv
)
{
    // load the files as specified by the input args
    auto filePaths = load_file_paths(argv[2]);
    if (filePaths.size() > workContractGroup->get_capacity())
        filePaths.resize(workContractGroup->get_capacity());

    // process all files in the input directory path using asynchronous tasks thanks to
    // our work_contract_group!
    std::atomic<std::size_t> completionCount{0};
    std::vector<std::shared_ptr<file_char_counter>> fileCharCounters;
    for (auto const & filePath : filePaths)
    {
        // create file char counter
        file_char_counter::configuration_type fileCharCounterConfiguration;
        fileCharCounterConfiguration.filePath_ = filePath;
        fileCharCounterConfiguration.target_ = argv[1][0];
        fileCharCounterConfiguration.endHandler_ = [&](auto const &)
                {
                    ++completionCount;
                };
        fileCharCounterConfiguration.beginHandler_ = [&](auto const & fileCharCounter)
                {
                    //std::cout << "Starting file " << fileCharCounter.get_file_path() << std::endl;
                };
        auto fileCharCounter = file_char_counter::create(*workContractGroup, fileCharCounterConfiguration);
        if (fileCharCounter)
            fileCharCounters.push_back(std::move(fileCharCounter));
        else
            ++completionCount; // failed to create the object. count as completed fro benefit of main thread exit.
    }

    // wait for all work to complete
    while (completionCount < fileCharCounters.size())
        std::this_thread::yield();

    // summary of results
    for (auto const & fileCharCounter : fileCharCounters)
    {
        std::cout << fileCharCounter->get_file_path() << " processed: '" << fileCharCounter->get_target() << "' occured " << 
                fileCharCounter->get_count() << " times in file" << std::endl;
    }
    std::cout << "processed " << fileCharCounters.size() << " files concurrently using " << num_threads << " worker threads." << std::endl;
    return 0;
}
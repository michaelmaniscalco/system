#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include <library/system.h>


namespace 
{
    using namespace maniscalco::system;

    std::condition_variable_any conditionVariable;
    std::mutex mutex;
    std::size_t invokationCounter{0};

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
            std::filesystem::path filePath_;
            char                                target_;

            begin_handler                       beginHandler_;
            end_handler                         endHandler_;
        };

        static std::shared_ptr<file_char_counter> create
        (
            work_contract_group & workContractGroup,
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
            work_contract_group & workContractGroup,
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
            workContract_ = {workContractGroup.create_contract(
                    {
                        .contractHandler_ = [wpThis = this->weak_from_this()] // invoke work contract
                                (
                                )
                                {
                                    if (auto sp = wpThis.lock(); sp)
                                        sp->process_data();
                                }
                    }),            
                    [&]()
                    {
                        {
                            std::unique_lock uniqueLock(mutex);
                            ++invokationCounter;
                        }
                        conditionVariable.notify_one();
                    }};
                    
            if (!workContract_.is_valid())
            {
                std::cerr << "Failed to create work contract" << std::endl;
                return false;
            }
            workContract_.invoke();
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
                workContract_.invoke(); // more to do so come back again!
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

        alertable_work_contract                 workContract_;

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
        std::filesystem::path directoryPath
    ) -> std::vector<std::filesystem::path>
    {
        std::vector<std::filesystem::path> results;
        for (auto const & filePath : std::filesystem::directory_iterator(directoryPath))
        {
            if (std::filesystem::is_directory(filePath))
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


} // anonymous namespace


int main
(
    int argc, 
    char const ** argv
)
{
    if (argc != 3)
    {
        std::cout << "usage:  file_io_test char_to_find path_to_input_directory\n";
        std::cout << "example (count occurances of character 'e' in all files in directory 'test_directory'):\n";
        std::cout << "\tfile_io_test e ~/test_directory/\n";
        return 0;
    }
    
    // create a work_contract_group - very simple
    auto workContractGroup = work_contract_group::create({});

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    std::vector<thread_pool::thread_configuration> threads(std::thread::hardware_concurrency());
    for (auto & thread : threads)
        thread.function_ = [&, previousInvokationCounter = 0](std::stop_token const & stopToken) mutable
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
                        };

    thread_pool workerThreadPool({.threads_ = threads});

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
    std::cout << "processed " << fileCharCounters.size() << " files concurrently using " << threads.size() << " worker threads." << std::endl;
    return 0;
}
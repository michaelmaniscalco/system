#include <condition_variable>
#include <cstddef>
#include <experimental/filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "./csv_parser.h"

#include <include/invoke_from_weak_ptr.h>
#include <library/system.h>

#include <boost/align/aligned_allocator.hpp>

#include <functional>
#include <atomic>
#include <typeinfo>
#include <cxxabi.h>


namespace test
{

    std::tuple<std::string, std::atomic<std::size_t> *, std::size_t>  instanceCounters_[1024];
    std::atomic<std::size_t> instanceCounterCount_;


    template <typename T>
    class InstanceCounter
    {
    public:

        static void register_self()
        {
            static bool const once = [](bool)
            {
                int status = -1;
                instanceCounters_[instanceCounterCount_++] = {abi::__cxa_demangle(typeid(T).name(), NULL, NULL, &status), &InstanceCounter<T>::count_, sizeof(T)}; 
                return true;
            }(once);
        }

        static void increment(){++count_;}
        static void decrement(){--count_;}
    protected:
    private:
        static std::atomic<std::size_t> count_;
        static char const * name_;
    };

    template <typename T> std::atomic<std::size_t> InstanceCounter<T>::count_;
    template <typename T> char const * InstanceCounter<T>::name_;


    template <typename T, typename ... args>
    std::shared_ptr<T> make_shared
    (
        args && ... arguments
    )
    {
        InstanceCounter<T>::register_self();
        InstanceCounter<T>::increment();
        return std::shared_ptr<T>(new T(std::forward<args>(arguments) ...), [](T * address){InstanceCounter<T>::decrement(); delete address;});
    }


    void print_instance_counts()
    {
        auto n = instanceCounterCount_.load();
        for (auto i = 0; i < n; ++i)
        {
            auto [name, count, size] = instanceCounters_[i];
            std::cout << "class " << name << ":  instances = " << *count << ", total memory consumption = " << (size * *count) << " bytes" << std::endl;
        }
    }
}


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
        for (auto const & directoryEntry : std::experimental::filesystem::directory_iterator(directoryPath))
        {
            if (std::experimental::filesystem::is_directory(directoryEntry))
            {
                auto subResults = load_file_paths(directoryEntry);
                results.insert(results.end(), subResults.begin(), subResults.end());
            }
            else
            {
                auto filePath = directoryEntry.path();
                if (filePath.extension() == ".csv")
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
/*
    std::condition_variable     conditionVariable;
    std::mutex                  mutex;
    
    // create a work_contract_group - very simple
    auto workContractGroup = maniscalco::system::work_contract_group::create(
            [&]()
            {
                // whenever a contract is excercised we use our condition variable to 'wake' a thread.
                conditionVariable.notify_one();
            });

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    auto num_threads = std::thread::hardware_concurrency();
    maniscalco::system::thread_pool workerThreadPool(
            {
                num_threads,
                [&]()
                {
                    // wait until the there is work to do rather than spin.
                    std::unique_lock uniqueLock(mutex);
                    std::chrono::microseconds waitTime(10);
                    if (conditionVariable.wait_for(uniqueLock, waitTime, [&](){return workContractGroup->get_service_requested();}))
                        workContractGroup->service_contracts();
                }
            });
*/

    static std::int32_t constexpr delimiter_distance[256] = 
    {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,

        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 

        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
        5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 8, 9
    };


#include <x86intrin.h>
#include <immintrin.h>
#include <emmintrin.h>
#include <avx2intrin.h>
#include <avx512vlbwintrin.h>


    std::size_t find_delimiters
    (
        char const * begin,
        char const * end
    )
    {
        static auto const __attribute__((aligned(32)))  tabs32 = _mm256_set_epi8(9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9);
        static auto const __attribute__((aligned(32)))  cr32 = _mm256_set_epi8(10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10);

        auto result = 0;
        auto cur = begin;
        auto tab_result = 0;
        auto cr_result = 0;

        while (cur < end)
        {
            static __m256i const mask = _mm256_set_epi64x(-1, -1, -1, -1);
            auto input = _mm256_load_si256((__m256i *)cur);
            _mm256_maskstore_epi32(&tab_result, mask, _mm256_cmpeq_epi8(tabs32, input));
            _mm256_maskstore_epi32(&cr_result, mask, _mm256_cmpeq_epi8(cr32, input));
            std::uint32_t all_result = ((unsigned)tab_result | (unsigned)cr_result);
            all_result = __builtin_bswap32(all_result);
            auto offset = 0;
            while (all_result)
            {
                auto n = __builtin_clz(all_result);
                offset += n;
                if (cur[offset] != 10)
                    std::cout << "ERROR " << std::endl;
                offset += n;
                all_result <<= (n + 1);
                ++result;
            }
            cur += 32;
        }
        return result;

      //  auto result = _mm_cmpeq_epi8_mask(evens, odds);
//std::cout << std::hex << result << std::endl;
  
//        std::int8_t const * r = (std::int8_t const *)&result;
//        for (auto i = 0; i < 32; ++i)
 //           std::cout << std::to_string(r[i]) << ", ";
 //       std::cout << std::endl;


        std::size_t delimiter_count = 0;
        static auto constexpr tab_mask = 0x0a0a0a0a0a0a0a0a;    
        while (begin < end)
        {
            // cast next 8 bytes
            auto input = __builtin_bswap64(*(std::uint64_t const *)begin);
            // mask for delimiters (0x00 equals matches delimiter)
            input ^= tab_mask;
            // produce LSB of each byte (0 equals byte matches delimiter, 1 equals not a match for delimiter)
            input |= (input >> 4);
            input |= (input >> 2);
            input |= (input >> 1);
            // mask off all but the LSB of each byte
            input &= 0x0101010101010101;
            if (input == 0x0101010101010101)
            {
                begin += 8;
                continue;
            }
            // consolidate LSBs into one 8 bit byte
            input |= ((input & 0x0100010001000100) >> 7);
            input |= ((input & 0x0003000000030000) >> 14);
            input |= ((input & 0x0000000f00000000) >> 28);
    
            auto distance = delimiter_distance[input & 0xff];
            begin += distance;
            delimiter_count++;
        }
        return delimiter_count;
    }


    //==================================================================================================================
    std::vector<char, boost::alignment::aligned_allocator<char, 32>> load_file
    (
        char const * path
    )
    {
        // read data from file
        std::vector<char, boost::alignment::aligned_allocator<char, 32>> input;
        std::ifstream inputStream(path, std::ios_base::in | std::ios_base::binary);
        if (!inputStream.is_open())
        {
            std::cout << "failed to open file \"" << path << "\"" << std::endl;
            return {};
       }

        inputStream.seekg(0, std::ios_base::end);
        std::size_t size = inputStream.tellg();
        input.resize(size);
        inputStream.seekg(0, std::ios_base::beg);
        inputStream.read(input.data(), input.size());
        inputStream.close();
        return input;
    }

} // anonymous namespace


class my_class{
    public:

    char _[8192];

};

int main
(
    int, 
    char const ** argv
)
{

        auto dummy = test::make_shared<int>(5);
    test::print_instance_counts();
    dummy.reset();
    test::print_instance_counts();

    std::vector<std::shared_ptr<my_class>> myClass;
    for (auto i = 0; i < 1024; ++i)
        myClass.push_back(test::make_shared<my_class>());

        test::print_instance_counts();

        myClass.clear();
               test::print_instance_counts(); 
/*
    std::int8_t delimiter_distance[256];
    for (auto i = 0; i < 0x100; ++i)
    {
        auto k = ((i >> 4) | ((i & 0x0f) << 4));
        std::size_t bit = 0x80;
        std::size_t distance = 1;
        while ((bit) && (k & bit))
        {
            ++distance;
            bit >>= 1;
        }
        delimiter_distance[i] = distance;
    }

    for (auto i = 0; i < 0x100; ++i)
        std::cout << std::to_string(delimiter_distance[i]) << ", ";// << std::endl;
    std::cout << std::endl;
    */
/*


    char test[8] = {'a', 'b', 'c', 'd', 'e', 0x09, 'f', 'g'};

    static auto constexpr tab_mask = 0x0909090909090909;
    auto input = *(std::uint64_t const *)test;

    // mask for delimiters (0x00 equals matches delimiter)
    input ^= tab_mask;

    // produce LSB of each byte (0 equals byte matches delimiter, 1 equals not a match for delimiter)
    input |= (input >> 4);
    input |= (input >> 2);
    input |= (input >> 1);

    // mask off all but the LSB of each byte
    input &= 0x0101010101010101;


    // consolidate LSBs into one 8 bit byte
    input |= ((input & 0x0100010001000100) >> 7);
    input |= ((input & 0x0003000000030000) >> 14);
    input |= ((input & 0x0000000f00000000) >> 28);


    std::cout << "distance to delimiter = " << std::to_string(delimiter_distance[input & 0xff]) << std::endl;
*/

    // load the files as specified by the input args
    auto filePaths = load_file_paths(argv[1]);

    std::size_t rowCount = 0;
    csv_parser csvParser
    ({
        [](auto const &, auto const & header)
                {
                 //   std::cout << "HEADER: ";
                 //   for (auto & field : header)
                 //       std::cout << std::string_view(field.begin(), field.size()) << "\t";
                 //   std::cout << std::endl;
                },// header event
        [&](auto const &, auto const & row)
                {
                    ++rowCount;
                 //   std::cout << "ROW " << std::to_string(rowCount) << ": ";
                 //   for (auto & field : row)
                 //       std::cout << std::string_view(field.begin(), field.size()) << "\t";
                 //   std::cout << std::endl;
                },   // row event
        [](auto const &){},                     // close event
         true,        // header expected
        '\t'           // delimiter
    }
    );

    auto input = load_file(filePaths[0].string().c_str());

    auto startTime = std::chrono::system_clock::now();
    auto delimiter_count = find_delimiters(input.data(), input.data() + ((input.size() & ~7)));
    auto finishTime = std::chrono::system_clock::now();
    auto elapsedTime = (finishTime - startTime);
    std::cout << "elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime).count() << " ms" << std::endl;

    std::cout << "delimiter count = " << delimiter_count << std::endl;

    return 0;
/*

    std::ifstream inputFileStream(filePaths[0], std::ios_base::in | std::ios_base::binary);
    if (!inputFileStream.is_open())
    {
        std::cerr << "failed to open file \"" << filePaths[0] << "\"" << std::endl;
        return 0;
    }

    static auto constexpr buffer_size = (1 << 20);
    std::array<char, buffer_size> buffer;
    char const * endOfBuffer = buffer.data() + buffer_size;
    char const * cur = buffer.data();
    char const * end = cur;
    auto bytesProcessed = 0;

    auto startTime = std::chrono::system_clock::now();
    while (!inputFileStream.eof())
    {
        end = std::copy(cur, end, buffer.data());
        inputFileStream.read((char *)end, endOfBuffer - end);
        auto bytesRead = inputFileStream.gcount();
        end += bytesRead;
        bytesProcessed += bytesRead;
        cur = csvParser.parse(buffer.data(), end);
    }
    auto finishTime = std::chrono::system_clock::now();
    auto elapsedTime = (finishTime - startTime);
    std::cout << "elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime).count() << " ms" << std::endl;
    std::cout << "row count = " << rowCount << std::endl;
    std::cout << "bytes processed: " << bytesProcessed << std::endl;
    return 0;
*/
/*
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
    */
}
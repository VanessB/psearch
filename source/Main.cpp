#include <cinttypes>
#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>

#include "Searcher.hpp"
#include "Walker.hpp"

int main(int argc, char* argv[])
{
    std::string pattern;
    std::cin >> pattern;

    std::shared_ptr<KMP> kmp(new KMP(pattern));

    uint64_t threads_number = 1;
    std::cin >> threads_number;

    // std::ifstream file("test");
    // std::vector<KMP::Entry> result;
    // kmp.search(file, result);
    // for (size_t i = 0; i < result.size(); ++i)
    // {
    //     std::cout << result[i].line_number << ": " << result[i].line << std::endl;
    // }

    Walker walker(kmp);
    std::filesystem::path current_path = std::filesystem::current_path();
    //std::filesystem::path current_path = std::filesystem::path("Test");
    std::thread walker_thread(&Walker::walk, &walker, std::ref(current_path));
    std::vector<std::thread> searcher_threads;
    for (size_t i = 0; i < threads_number; ++i)
    {
        searcher_threads.push_back(std::thread(&Walker::search, std::ref(walker)));
    }

    walker_thread.join();
    for (size_t i = 0; i < threads_number; ++i)
    {
        searcher_threads[i].join();
    }
    return 0;
}

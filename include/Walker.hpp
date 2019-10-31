#ifndef PSEARCH_WALKER
#define PSEARCH_WALKER
#include <filesystem>
#include <fstream>
#include <queue>
#include <thread>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

#include "Searcher.hpp"

////////////////     Walker     ////////////////
// Класс для рекурсивного параллельного поиска.
class Walker
{
public:
    Walker(std::shared_ptr<Searcher> init_searcher);

    Walker(Walker&& other) = delete;
    Walker(const Walker& other) = delete;
    Walker& operator =(Walker&& other) = delete;
    Walker& operator =(const Walker& other) = delete;

    void walk(std::filesystem::path& walk_path);
    void search();

protected:
    std::shared_ptr<Searcher> searcher;      // Класс для поиска в потоке ввода.
    std::queue<std::filesystem::path> files; // Очередь файлов для обработки.
    std::shared_mutex mutex_files;           // Разделяемый mutex для работы с очередью.
    std::condition_variable condition_files; // Переменная состояния для работы с очередью.
    std::mutex mutex_condition_files;        // mutex, отведённый для работы condition_files.
    std::atomic<bool> walking = false;       // Переменная, сигнализирующая о происходящем обходе директорий.
    std::mutex mutex_output;                 // mutex для вывода найденных вхождений.

    size_t walk_buffer_size = 64;                    // Размер буффера путей файлов, ожидающих добавления в очередь.
    size_t search_max_buffer_size = 1024 * 1024 * 4; // Минимальный суммарный вес файлов перед началом поииска.

private:

};

#endif

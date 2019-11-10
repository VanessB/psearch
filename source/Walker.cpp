#include "Walker.hpp"
#include <iostream>

//#define DEBUG_OUTPUT_WALKER_WALK

////////////////     Walker     ////////////////
// Класс для рекурсивного параллельного поиска.
// PUBLIC:
Walker::Walker(std::shared_ptr<Searcher> init_searcher)
{
    searcher = init_searcher;
}

void Walker::walk(std::filesystem::path& walk_path, bool recursively)
{
    walking.store(true);
    std::vector<std::filesystem::path> buffer;

    #ifdef DEBUG_OUTPUT_WALKER_WALK
    std::cout << "Обнаруженные файлы: " << std::endl;
    #endif

    // TODO: сделать что-то с этим тупым копипастом кода.
    // Рекурсивный обход директори.
    if (recursively)
    {
        for (auto& path: std::filesystem::recursive_directory_iterator(walk_path))
        {
            // Проверка, является ли найденный файл обычным.
            if (std::filesystem::is_regular_file(path))
            {
                // Добавление файла в буффер.
                #ifdef DEBUG_OUTPUT_WALKER_WALK
                std::cout << path << std::endl;
                #endif
                buffer.push_back(path);

                // Если буффер наполнился, происходит его сброс в общую очередь.
                if (buffer.size() >= walk_buffer_size)
                {
                    std::unique_lock<std::shared_mutex> lock(mutex_files);
                    for (auto& iter: buffer) { files.push(iter); }
                    buffer.clear();
                    condition_files.notify_all(); // Оповещение, что в очередь добавлены новые файлы.
                }
            }
        }
    }
    // Нерекурсивный обход директории.
    else
    {
        for (auto& path: std::filesystem::directory_iterator(walk_path))
        {
            // Проверка, является ли найденный файл обычным.
            if (std::filesystem::is_regular_file(path))
            {
                // Добавление файла в буффер.
                #ifdef DEBUG_OUTPUT_WALKER_WALK
                std::cout << path << std::endl;
                #endif
                buffer.push_back(path);

                // Если буффер наполнился, происходит его сброс в общую очередь.
                if (buffer.size() >= walk_buffer_size)
                {
                    std::unique_lock<std::shared_mutex> lock(mutex_files);
                    for (auto& iter: buffer) { files.push(iter); }
                    buffer.clear();
                    condition_files.notify_all(); // Оповещение, что в очередь добавлены новые файлы.
                }
            }
        }
    }

    // Сброс остатка буффера в очередь.
    {
        std::unique_lock<std::shared_mutex> lock(mutex_files);
        for (auto& iter: buffer) { files.push(iter); }
        buffer.clear();
        condition_files.notify_all(); // Оповещение, что в очередь добавлены новые файлы.
    }

    // Оповещение об окончании обхода.
    walking.store(false);
    condition_files.notify_all();

    #ifdef DEBUG_OUTPUT_WALKER_WALK
    std::cout << "Обход завершён. " << std::endl;
    #endif
}

void Walker::search()
{
    std::vector<Searcher::Entry> entries;
    std::queue<std::filesystem::path> buffer;
    while (true)
    {
        while (true)
        {
            {
                size_t current_buffer_size = 0;
                std::unique_lock<std::shared_mutex> lock(mutex_files);

                // При непустой очереди следует начать заполнение буффера файлами.
                if (!files.empty())
                {
                    // Заполнение буффера.
                    while (!files.empty() && current_buffer_size <= search_max_buffer_size)
                    {
                        buffer.push(files.front());
                        files.pop();
                        current_buffer_size += std::filesystem::file_size(buffer.back());
                    }
                }
                else
                { break; }
            }

            while (!buffer.empty())
            {
                std::filesystem::path current_file = buffer.front();
                std::ifstream file_stream(current_file);
                searcher->search(file_stream, entries);
                buffer.pop();

                if (!entries.empty())
                {
                    std::unique_lock<std::mutex> lock(mutex_output);
                    for (auto entry : entries)
                    { std::cout << current_file << "\t " << entry.line_number << ": " << entry.line << std::endl; }
                    entries.clear();
                }
            }
        }

        {
            bool wait = false;
            {
                std::shared_lock<std::shared_mutex> lock(mutex_files);
                if (files.empty())
                {
                    // Если очередь пустая, а обхода нет, цикл завершается.
                    if (!walking.load()) { break; }
                    else { wait = true; }
                }
            }
            if (wait)
            {
                // Если очередь пустая, а обход есть, производится переход в режим ожидания.
                std::unique_lock<std::mutex> wait_lock(mutex_condition_files);
                condition_files.wait(wait_lock);
            }
        }
    }
}

// PROTECTED:

// PRIVATE:

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

//#define DEBUG_OUTPUT_SEARCHER_SEARCH
//#define DEBUG_OUTPUT_WALKER_WALK


////////////////    Searcher    ////////////////
// Класс-интерфейс для всех объектов, предоставляющих функциональность поиска.
class Searcher
{
public:
    // Структура для хранения информации о найденных вхождениях образца.
    struct Entry
    {
        size_t line_number;
        std::string line;
        uint32_t entries_number;
    };

    //Searcher();
    //~Searcher();

    virtual void search(std::istream& stream, std::vector<Entry>& entries) const = 0;

protected:

private:

};

////////////////       KMP       ///////////////
// Класс, реализующий автомат Кнута-Морриса-Пратта.
class KMP : public Searcher
{
public:
    KMP(const std::string& init_string)
    {
        pattern = init_string;

        // Создание в куче пустого вектора для автомата.
        const size_t str_size = pattern.size();
        const size_t char_size = (sizeof(char) << 8);
        const size_t kmp_size = (str_size + 1) * char_size;
        states_table = std::vector<size_t>(kmp_size, 0);

        // Предвычисление префикс-функции для данной строки.
        std::vector<size_t> pi(str_size, 0);
        for (size_t i = 1; i < str_size; i++)
        {
            int j = pi[i-1];
            while (j > 0 && pattern[i] != pattern[j])
                j = pi[j-1];
            if (pattern[i] == pattern[j])
                j++;
            pi[i] = j;
        }

        // Вычисление перехода для каждого состояния, кроме последнего, и символа.
        for (size_t state = 0; state < str_size; ++state)
        {
            for (char ch = 0; static_cast<size_t>(static_cast<unsigned char>(ch)) < 255; ++ch)
            {
                // Вычисление перехода по префикс-функции.
                size_t next_state = state;
                while (true)
                {
                    if (pattern[next_state] == ch)
                    { ++next_state; break; }
                    if (next_state) { next_state = pi[next_state - 1]; }
                    else { break; }
                }

                // Запись в таблицу.
                states_table[state * char_size + static_cast<size_t>(static_cast<unsigned char>(ch))] = next_state;
            }
        }

        for (char ch = 0; static_cast<size_t>(static_cast<unsigned char>(ch)) < 255; ++ch)
        {
            // Вычисление перехода по префикс-функции.
            size_t next_state = str_size;
            while (true)
            {
                if (next_state) { next_state = pi[next_state - 1]; }
                else { break; }
            }

            // Запись в таблицу.
            states_table[str_size * char_size + static_cast<size_t>(static_cast<unsigned char>(ch))] = next_state;
        }
    }

    void search(std::istream& stream, std::vector<Searcher::Entry>& entries) const
    {
        // Некоторые константы.
        const size_t char_size = (sizeof(char) << 8);
        const size_t str_size = pattern.size();

        // Вспомогательные переменные.
        size_t state = 0;         // Текущее состояние автомата.
        char ch = 0;              // Прочитанный символ.
        //size_t offset = 0;        // Смещение в строке.
        //size_t global_offset = 0; // Глобальное смещение.

        // Структура для сохранения информации о вхождении.
        Searcher::Entry entry;
        entry.line_number = 0;
        entry.entries_number = 0;

        // Цикл поиска.
        while(stream.get(ch))
        {
            //std::cout << int((unsigned char)(ch)) << std::endl;
            // Обработка перехода на новую строку.
            if (ch == '\n')
            {
                //offset = 0;
                if (entry.entries_number) { entries.push_back(entry); }
                entry.entries_number = 0;
                ++entry.line_number;
                entry.line.clear();

                #ifdef DEBUG_OUTPUT_SEARCHER_SEARCH
                std::cout << "Line:" << entry.line_number << std::endl;
                #endif
            }
            else { entry.line.push_back(ch); }
            //line += (ch == '\n');
            //offset *= (ch != '\n');

            state = states_table[state * char_size + static_cast<size_t>(static_cast<unsigned char>(ch))];
            if (state == str_size)
            {
                //std::cout << "Подстрока найдена на " << line << " строке со смещением " << offset - str_size << std::endl;
                ++entry.entries_number;
            }
            //++offset;
            //++global_offset;
        }
    }


protected:
    std::string pattern;              // Строка-образец.
    std::vector<size_t> states_table; // Таблица состояний.

private:

};



////////////////     Walker     ////////////////
// Класс для рекурсивного параллельного поиска.
class Walker
{
public:
    Walker(std::shared_ptr<Searcher> init_searcher)
    {
        searcher = init_searcher;
    }

    Walker(Walker&& other) = delete;
    Walker(const Walker& other) = delete;
    Walker& operator =(Walker&& other) = delete;
    Walker& operator =(const Walker& other) = delete;

    void walk(std::filesystem::path& walk_path)
    {
        walking.store(true);
        std::vector<std::filesystem::path> buffer;

        #ifdef DEBUG_OUTPUT_WALKER_WALK
        std::cout << "Обнаруженные файлы: " << std::endl;
        #endif

        // Рекурсивнй обход по директориям.
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

    void search()
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

        {
            std::unique_lock<std::mutex> lock(mutex_output);
            for (auto entry : entries)
            { std::cout << entry.line_number << ": " << entry.line << std::endl; }
        }
    }

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

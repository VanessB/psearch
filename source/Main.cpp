#include <cinttypes>
#include <memory>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <string>
#include <vector>
#include <exception>
//#include <chrono>

#include "Searcher.hpp"
#include "Walker.hpp"

class invalid_arguments : std::exception
{
public:
    enum class code
    {
        ok,           // ОК.
        unknown,      // Неизвестные аргументы.
        invalid,      // Неверно заданный аргумент.
        incompatable, // Несовместимые аргументы.
        missing,      // Отсутствует обязательный аргумент.
    };

    explicit invalid_arguments(code init_code) noexcept
    {
        _code = init_code;
        switch (_code)
        {
            case code::ok: { break; }
            case code::unknown:      { message = "Неизвестный аргумент."; break; }
            case code::invalid:      { message = "Неверно заданный аргумент."; break; }
            case code::incompatable: { message = "Несовместимые аргументы."; break; }
            case code::missing:      { message = "Отсутствует необходимый аргумент."; break; }
        }
    }
    explicit invalid_arguments(code init_code, const std::string& additional) noexcept
    {
        _code = init_code;
        switch (_code)
        {
            case code::ok: { break; }
            case code::unknown:      { message = "Неизвестный аргумент: " + additional; break; }
            case code::invalid:      { message = "Неверно заданный аргумент: " + additional; break; }
            case code::incompatable: { message = "Несовместимые аргументы: " + additional; break; }
            case code::missing:      { message = "Отсутствует необходимый аргумент: " + additional; break; }
        }
    }

    virtual const char* what() const noexcept
    {
        return message.c_str();
    }

protected:
    std::string message;
    code _code;

private:

};

int main(int argc, char* argv[])
{
    int threads_number = -1;
    bool recursively = true;
    std::string pattern;
    std::string path_str;
    bool benchmark = false;

    try
    {
        // Получение обязательного аргумента: образца.
        if (argc < 2) { throw invalid_arguments(invalid_arguments::code::missing, "pattern (образец)."); }
        pattern = std::string(argv[1]);

        // Получение дополнительных аргументов.
        for (int arg_i = 2; arg_i < argc; ++arg_i)
        {
            std::string argument(argv[arg_i]);

            // Обработка ключей.
            if (argument[0] == '-')
            {
                // Ключ числа потоков.
                if (argument[1] == 't')
                {
                    // Если число потоков уже задано, выброс исключения.
                    if (threads_number > 0)
                    { throw invalid_arguments(invalid_arguments::code::incompatable, argument + " (число потоков уже было передано в качестве аргумента)."); }

                    // Перевод значения ключа в число. При неудаче - выброс исключения.
                    try
                    { threads_number = std::stoi(argument.substr(2, argument.size() - 1)); }
                    catch (const std::logic_error& eception)
                    { throw invalid_arguments(invalid_arguments::code::invalid, argument + " (ожидалось число)."); }

                    // Если число потоков некорректно, выброс исключения.
                    if (threads_number < 1)
                    { throw invalid_arguments(invalid_arguments::code::invalid, argument + " (число потоков строго положительно)."); }
                }
                // Ключ нерекурсивного поиска.
                else if (argument == "-n")
                {
                    if (recursively)
                    { recursively = false; }
                    else
                    { throw invalid_arguments(invalid_arguments::code::incompatable, argument + " (ключ нерекурсивного поиска уже был передан в качестве аргумента)"); }
                }
                // Ключ измерения времени выполнения.
                else if (argument == "-b")
                {
                    if (!benchmark)
                    { benchmark = true; }
                    else
                    { throw invalid_arguments(invalid_arguments::code::incompatable, argument + " (ключ измерения времени выполнения уже был передан в качестве аргумента)"); }
                }
                else
                { throw invalid_arguments(invalid_arguments::code::unknown, argument); }
            }
            else
            {
                // Оставшийся аргумент - путь поиска.
                if (!path_str.empty())
                { throw invalid_arguments(invalid_arguments::code::incompatable, argument + " (путь поиска уже был передан в качестве аргумента)."); }
                path_str = argument;
            }
        }
    }
    catch (const invalid_arguments &exception)
    {
        std::cerr << exception.what() << std::endl;
        return 1;
    }

    // Обработка ситуации, когда число потоков передано не было.
    if (threads_number < 0) { threads_number = 1; }

    // Измерение времени выполнения.
    std::clock_t timestamp_started = std::clock();

    // Создание КМП-автомата для образца.
    std::shared_ptr<KMP> kmp(new KMP(pattern));

    // Создание объекта для поиска.
    Walker walker(kmp);

    // Выбор директории для поиска.
    std::filesystem::path search_path;
    if (!path_str.empty())
    { search_path = std::filesystem::path(path_str); }
    else
    { search_path = std::filesystem::current_path(); }

    // Запуск потока для обхода директорий.
    std::thread walker_thread(&Walker::walk, &walker, std::ref(search_path), std::ref(recursively));

    // Запуск потоков для поиска в файлах.
    std::vector<std::thread> searcher_threads;
    for (int i = 0; i < threads_number; ++i)
    {
        searcher_threads.push_back(std::thread(&Walker::search, std::ref(walker)));
    }

    // Ожидание завершения работы потоков.
    walker_thread.join();
    for (int i = 0; i < threads_number; ++i)
    {
        searcher_threads[i].join();
    }

    std::clock_t timestamp_finished = std::clock();
    if (benchmark)
    {
        std::cout << std::fixed << std::setprecision(2)
                  << "[BENCHMARK]: " << std::endl
                  << "CPU: "
                  << 1000.0 * (timestamp_finished - timestamp_started) / CLOCKS_PER_SEC << "ms" << std::endl
                  << "CPU средн.:"
                  << 1000.0 * (timestamp_finished - timestamp_started) / CLOCKS_PER_SEC / threads_number << "ms" << std::endl
                  << std::defaultfloat;
    }

    return 0;
}

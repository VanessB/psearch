#include "Searcher.hpp"
#include <iostream>

//#define DEBUG_OUTPUT_SEARCHER_SEARCH

////////////////       KMP       ///////////////
// Класс, реализующий автомат Кнута-Морриса-Пратта.
// PUBLIC:
KMP::KMP(const std::string& init_string)
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

void KMP::search(std::istream& stream, std::vector<Searcher::Entry>& entries) const
{
    // Некоторые константы.
    const size_t char_size = (sizeof(char) << 8);
    const size_t str_size = pattern.size();

    // Вспомогательные переменные.
    size_t state = 0; // Текущее состояние автомата.

    // Структура для сохранения информации о вхождении.
    Searcher::Entry entry;
    entry.line_number = 0;
    entry.entries_number = 0;

    // Цикл поиска.
    while(std::getline(stream, entry.line))
    {
        for (size_t i = 0; i < entry.line.size(); ++i)
        {
            state = states_table[state * char_size + static_cast<size_t>(static_cast<unsigned char>(entry.line[i]))];
            if (state == str_size)
            { ++entry.entries_number; }
            //entry.entries_number += (state == str_size);
        }

        if (entry.entries_number) { entries.push_back(entry); }
        entry.entries_number = 0;
        ++entry.line_number;
        entry.line.clear();

        #ifdef DEBUG_OUTPUT_SEARCHER_SEARCH
        std::cout << "Line:" << entry.line_number << std::endl;
        #endif
    }
}

// PROTECTED:

// PRIVATE:

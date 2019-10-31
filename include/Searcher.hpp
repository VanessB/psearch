#ifndef PSEARCH_SEARCHER
#define PSEARCH_SEARCHER
#include <vector>
#include <string>

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

    virtual void search(std::istream& stream, std::vector<Entry>& entries) const = 0;

protected:

private:

};

////////////////       KMP       ///////////////
// Класс, реализующий автомат Кнута-Морриса-Пратта.
class KMP : public Searcher
{
public:
    KMP(const std::string& init_string);

    void search(std::istream& stream, std::vector<Searcher::Entry>& entries) const;

protected:
    std::string pattern;              // Строка-образец.
    std::vector<size_t> states_table; // Таблица состояний.

private:

};

#endif

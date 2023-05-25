#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <cassert>

using namespace std::string_literals;

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
    const std::string& hint) {
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <class T>
void RunTestImpl(T func, const std::string& name) {
    /* Напишите недостающий код */
    func();
    std::cerr << name << " OK" << std::endl;
}

#define RUN_TEST(func)  RunTestImpl(func, #func)


template<class T>
void Print(std::ostream& os, const T& container)
{
    bool is_first = true;
    for (const auto& element : container) {
        if (!is_first) {
            os << ", "s;
        }
        is_first = false;
        os << element;
    }
}

template<class T, class U>
std::ostream& operator<<(std::ostream& os, const std::pair<T, U>& element)
{
    return os << element.first << ": " << element.second;
}


template<class T, class U>
std::ostream& operator<<(std::ostream& os, const std::map<T, U>& elements)
{
    os << "{"s;
    Print(os, elements);
    return os << "}"s;
}

template<class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& elements)
{
    os << "[";
    Print(os, elements);
    return os << "]";
}

template<class T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& elements)
{
    os << "{"s;
    Print(os, elements);
    return os << "}"s;
}

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

std::string ReadLine() 
{
    std::string s;
    std::getline(std::cin, s);
    return s;
}

int ReadLineWithNumber() 
{
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}

std::vector<std::string> SplitIntoWords(const std::string& text) 
{
    std::vector<std::string> words;
    std::string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
            word += c;
    }

    if (!word.empty())
        words.push_back(word);

    return words;
}

struct Document 
{
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus 
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer 
{
public:
    void SetStopWords(const std::string& text) 
    {
        for (const std::string& word : SplitIntoWords(text))
            stop_words_.insert(word);
    }

    void AddDocument(int document_id, const std::string& document, DocumentStatus status,const std::vector<int>& ratings) 
    {
        const std::vector<std::string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const std::string& word : words)
            word_to_document_freqs_[word][document_id] += inv_word_count;
        
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    std::vector<Document> FindTopDocuments(const std::string& raw_query, const DocumentStatus status = DocumentStatus::ACTUAL) const
    {
        return FindTopDocuments(raw_query, 
            [status]([[maybe_unused]]int id, [[maybe_unused]]DocumentStatus status_to_compare, [[maybe_unused]]int rating)
            {
                return status == status_to_compare;
            });
    }

    template <class T>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, T predicate) const 
    {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) 
            {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON)
                    return lhs.rating > rhs.rating;
                else 
                    return lhs.relevance > rhs.relevance;
            });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);

        return matched_documents;
    }

    inline int GetDocumentCount() const 
    {
        return static_cast<int>(documents_.size());
    }

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const 
    {
        const Query query = ParseQuery(raw_query);
        std::vector<std::string> matched_words;
        for (const std::string& word : query.plus_words) 
        {
            if (word_to_document_freqs_.count(word) == 0)
                continue;
            if (word_to_document_freqs_.at(word).count(document_id))
                matched_words.push_back(word);
        }
        for (const std::string& word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
                continue;
            
            if (word_to_document_freqs_.at(word).count(document_id)) 
            {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData 
    {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;

    inline bool IsStopWord(const std::string& word) const 
    {
        return stop_words_.count(word) > 0;
    }

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const 
    {
        std::vector<std::string> words;
        for (const std::string& word : SplitIntoWords(text))
            if (!IsStopWord(word))
                words.push_back(word);
        return words;
    }

    static int ComputeAverageRating(const std::vector<int>& ratings) 
    {
        if (ratings.empty())
            return 0;

        int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord 
    {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string text) const 
    {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') 
        {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query 
    {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const 
    {
        Query query;
        for (const std::string& word : SplitIntoWords(text)) 
        {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) 
            {
                if (query_word.is_minus)
                    query.minus_words.insert(query_word.data);
                else
                    query.plus_words.insert(query_word.data);
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const std::string& word) const 
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <class T>
    std::vector<Document> FindAllDocuments(const Query& query, T predicate) const 
    {
        std::map<int, double> document_to_relevance;
        for (const std::string& word : query.plus_words) 
        {
            if (word_to_document_freqs_.count(word) == 0)
                continue;

            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) 
            {
                const auto& [rating, status] = documents_.at(document_id);
                if (predicate(document_id, status, rating))
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }

        for (const std::string& word : query.minus_words) 
        {
            if (word_to_document_freqs_.count(word) == 0)
                continue;

            for (const auto& [document_id, _] : word_to_document_freqs_.at(word))
                document_to_relevance.erase(document_id);
        }

        std::vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance)
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        return matched_documents;
    }
};

void PrintDocument(const Document& document) 
{
    std::cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << std::endl;
}

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        assert(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        assert(doc0.id == doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        assert(server.FindTopDocuments("in"s).empty());
    }
}

void TestDocumentExistence()
{
    const int doc_id = 3;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    
    
    {
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT(found_docs.size() == 1);
        ASSERT(found_docs[0].id == 3);
    }

    {
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT(found_docs.size() == 0);
    }
}

void TestMinusWordsExclusion()
{
    const int doc_id = 3;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    // Проверяем, что поиск по-прежнему работает для документов с минус-словами
    {
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT(found_docs.size() == 1);
        ASSERT(found_docs[0].id == 3);
    }

    // Проверяем, что минус-слова исключаются
    {
        const auto found_docs = server.FindTopDocuments("-city"s);
        ASSERT(found_docs.size() == 0);
    }
}

void TestDocumentMatching()
{
    const int doc_id = 3;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    {
        const std::string query = "cat";
        const auto [matched_words, status] = server.MatchDocument(query, doc_id);

        ASSERT(matched_words.size() == 1);
        ASSERT(matched_words[0] == "cat");
    }

    {
        const std::string query = "cat -city";
        const auto [matched_words, status] = server.MatchDocument(query, doc_id);

        ASSERT(matched_words.size() == 0);
    }

}

void TestAverageRating()
{
    const int doc_id = 3;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    const std::string query = "cat";
    auto docs = server.FindTopDocuments(query);
    
    ASSERT(docs[0].rating == 2);
}

void TestRelevanceSort()
{
    SearchServer search_server;
    search_server.SetStopWords("and in on"s);
    search_server.AddDocument(0, "white cat and nice collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::IRRELEVANT, { 7, 2, 7 });
    search_server.AddDocument(2, "well-groomed dog expressive eyes"s, DocumentStatus::REMOVED, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "well-groomed starling evgeny"s, DocumentStatus::BANNED, { 9 });

    const auto& documents = search_server.FindTopDocuments("fluffy well-groomed cat"s);

    ASSERT(documents.size() != 0);

    for (size_t i = 1; i < documents.size() - 1; ++i)
        ASSERT(documents[i].relevance > documents[i - 1].relevance);

}

void TestStatusFilter()
{
    SearchServer search_server;
    search_server.AddDocument(0, "white cat and nice collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::IRRELEVANT, { 7, 2, 7 });
    search_server.AddDocument(2, "well-groomed cat expressive eyes"s, DocumentStatus::REMOVED, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "well-groomed cat evgeny"s, DocumentStatus::BANNED, { 9 });

    {
        const auto& documents = search_server.FindTopDocuments("cat"s, DocumentStatus::BANNED);

        ASSERT(documents.size() == 1);
        ASSERT(documents[0].id == 3);
    }

    {
        const auto& documents = search_server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);

        ASSERT(documents.size() == 1);
        ASSERT(documents[0].id == 1);
    }

    {
        const auto& documents = search_server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);

        ASSERT(documents.size() == 1);
        ASSERT(documents[0].id == 0);
    }
}

void TestRelevancyAccuracy()
{
    SearchServer search_server;
    search_server.SetStopWords("and in on"s);
    search_server.AddDocument(0, "white cat and nice collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });

    const double relevance_id0 = 0.173287;
    const double relevance_id1 = 0.866434;

    const auto& docs = search_server.FindTopDocuments("fluffy well-groomed cat"s);

    ASSERT(docs.size() == 2);
    ASSERT(docs[0].relevance - relevance_id1 < EPSILON);
    ASSERT(docs[1].relevance - relevance_id0 < EPSILON);
}

void TestUserPredicate()
{
    SearchServer search_server;
    search_server.SetStopWords("and in on"s);
    search_server.AddDocument(0, "white cat and nice collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "well-groomed dog expressive eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "well-groomed starling evgeny"s, DocumentStatus::BANNED, { 9 });

    {
        const auto& documents = search_server.FindTopDocuments("fluffy well-groomed cat"s,
            [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
        
        ASSERT(documents.size() == 1);
        ASSERT(documents[0].id == 3);
    }

    {
        const auto& documents = search_server.FindTopDocuments("fluffy well-groomed cat"s,
            [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });

        ASSERT(documents.size() == 2);
        ASSERT(documents[0].id == 0);
        ASSERT(documents[1].id == 2);
    }

}

/*
Разместите код остальных тестов здесь
*/

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    // Поддержка стоп-слов
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestDocumentExistence);
    RUN_TEST(TestMinusWordsExclusion);
    RUN_TEST(TestDocumentMatching);
    RUN_TEST(TestRelevanceSort);
    RUN_TEST(TestAverageRating);
    RUN_TEST(TestStatusFilter);
    RUN_TEST(TestRelevancyAccuracy);
    RUN_TEST(TestUserPredicate);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() 
{
    TestSearchServer();
    //SearchServer search_server;
    //search_server.SetStopWords("и в на"s);
    //search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    //search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    //search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    //search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    //
    //std::cout << "ACTUAL by default:"s << std::endl;
    //for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s))
    //    PrintDocument(document);
    //
    //std::cout << "BANNED:"s << std::endl;
    //for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED))
    //    PrintDocument(document);
    //
    //std::cout << "Even ids:"s << std::endl;
    //for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, 
    //    [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
    //    PrintDocument(document);

    return 0;
}

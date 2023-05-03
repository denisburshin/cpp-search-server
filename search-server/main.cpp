#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

//Оставил только потому, что без этого не проходит тест в тренажере
using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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
        return FindTopDocuments(raw_query, [status](int id, DocumentStatus status_to_compare, int rating)
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
                if (abs(lhs.relevance - rhs.relevance) < 1e-6)
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

        int rating_sum = 0;
        for (const int rating : ratings)
            rating_sum += rating;
 
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

using namespace std::string_literals;

void PrintDocument(const Document& document) 
{
    std::cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << std::endl;
}

int main() 
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    
    std::cout << "ACTUAL by default:"s << std::endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s))
        PrintDocument(document);

    std::cout << "BANNED:"s << std::endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED))
        PrintDocument(document);
    
    std::cout << "Even ids:"s << std::endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, 
        [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; }))
        PrintDocument(document);

    return 0;
}

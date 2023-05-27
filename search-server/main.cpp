#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <cassert>
#include <stdexcept>
#include <algorithm>
#include <cmath>

using namespace std::string_literals;
// Для тренажера
using namespace std;

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

    Document()
        : id(0), relevance(0.0), rating(0)
    {

    }

    Document(int id, double relevance, int rating)
        : id(id), relevance(relevance), rating(rating)
    {}
};

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string> non_empty_strings;
    for (const std::string& str : strings) {
        if (!str.empty())
        {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

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
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
    {
        const auto& set_stop_words = MakeUniqueNonEmptyStrings(stop_words);
        if (!std::all_of(set_stop_words.begin(), set_stop_words.end(), IsValidWord))
                throw std::invalid_argument("Stop word has forbidden symbols");

        stop_words_ = set_stop_words;
    }

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings)
    {
        if (document_id < 0 || documents_.count(document_id))
            throw std::invalid_argument("Document ID " + std::to_string(document_id) + " less than zero or already exists");

        const std::vector<std::string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const std::string& word : words)
        {
            if (!IsValidWord(word))
                throw std::invalid_argument("Word " + word + " has forbidden symbols in document " + std::to_string(document_id));

            word_to_document_freqs_[word][document_id] += inv_word_count;
        }

        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        documents_id_.push_back(document_id);
    }

    std::vector<Document> FindTopDocuments(const std::string& raw_query, const DocumentStatus status) const
    {
        return FindTopDocuments(raw_query,
            [status]([[maybe_unused]] int id, [[maybe_unused]] DocumentStatus status_to_compare, [[maybe_unused]] int rating)
            {
                return status == status_to_compare;
            });
    }

    template <class T>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, T predicate) const
    {

        Query query = ParseQuery(raw_query);

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

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const
    {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    inline int GetDocumentCount() const
    {
        return static_cast<int>(documents_.size());
    }

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const
    {
        Query query = ParseQuery(raw_query);

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

    inline int GetDocumentId(int index) const
    {
        return documents_id_.at(index);
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
    std::vector<int> documents_id_;

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

            if (text.empty() || text[0] == '-')
                throw std::invalid_argument("Minus word is empty or has extra minus sign");

        }

        if (!IsValidWord(text))
            throw std::invalid_argument("Minus word is empty or has extra minus sign");

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
            QueryWord query_word = ParseQueryWord(word);

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

    static bool IsValidWord(const std::string& word) {
        // A valid word must not contain special characters
        return std::none_of(word.begin(), word.end(),
            [](char c)
            {
                return c >= '\0' && c < ' ';
            });
    }
};

#pragma once

#include "document.h"
#include "string_processing.h"

#include <map>
#include <cmath>
#include <algorithm>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer
{
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    explicit SearchServer(const std::string& stop_words_text);

    template <class StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    std::vector<Document> FindTopDocuments(const std::string& raw_query, const DocumentStatus status) const;

    template <class Predicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Predicate predicate) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;
    
    inline int GetDocumentCount() const
    {
        return static_cast<int>(documents_.size());
    }

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;
   
    inline std::set<int>::const_iterator begin() const
    {
        return documents_id_.cbegin();
    }

    inline std::set<int>::const_iterator end() const
    {
        return documents_id_.cend();
    }

    void RemoveDocument(int document_id);

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

private:
    static bool IsValidWord(const std::string& word);

    inline bool IsStopWord(const std::string& word) const
    {
        return stop_words_.count(word) > 0;
    }

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    struct Query;
    Query ParseQuery(const std::string& text) const;

    struct QueryWord;
    QueryWord ParseQueryWord(std::string text) const;

    template <class Predicate>
    std::vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const;
private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    struct QueryWord
    {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query
    {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    std::set<std::string> stop_words_;
    std::map<int, std::map<std::string, double>> id_to_word_freqs_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> documents_id_;
};


template <class Predicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, Predicate predicate) const
{
    Query query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, predicate);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs)
        {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON)
                return lhs.rating > rhs.rating;
            else
                return lhs.relevance > rhs.relevance;
        });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);

    return matched_documents;
}

template <class StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
{
    const auto& set_stop_words = MakeUniqueNonEmptyStrings(stop_words);
    if (!std::all_of(set_stop_words.begin(), set_stop_words.end(), IsValidWord))
        throw std::invalid_argument("Stop word has forbidden symbols");

    stop_words_ = set_stop_words;
}

template <class Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Predicate predicate) const
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
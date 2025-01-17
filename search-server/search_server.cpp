#include "search_server.h"

#include <numeric>

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings)
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
        id_to_word_freqs_[document_id][word] += inv_word_count;
    }

    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    documents_id_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, const DocumentStatus status) const
{
    return FindTopDocuments(raw_query,
        [status]([[maybe_unused]] int id, [[maybe_unused]] DocumentStatus status_to_compare, [[maybe_unused]] int rating)
        {
            return status == status_to_compare;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const
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


std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const
{
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text))
        if (!IsStopWord(word))
            words.push_back(word);
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
    if (ratings.empty())
        return 0;

    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const
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

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const
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

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    if (id_to_word_freqs_.count(document_id))
        return id_to_word_freqs_.at(document_id);

    static std::map<std::string, double> dummy;

    return dummy;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const
{
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void SearchServer::RemoveDocument(int document_id)
{
    documents_.erase(document_id);
    documents_id_.erase(document_id);

    if (id_to_word_freqs_.count(document_id))
    {
        for (const auto& [word, _] : id_to_word_freqs_.at(document_id))
            if (word_to_document_freqs_.count(word))
                word_to_document_freqs_[word].erase(document_id);
    }

    id_to_word_freqs_.erase(document_id);
}

bool SearchServer::IsValidWord(const std::string& word)
{
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(),
        [](char c)
        {
            return c >= '\0' && c < ' ';
        });
}
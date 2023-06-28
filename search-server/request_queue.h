#pragma once
#include "search_server.h"

#include <deque>

class RequestQueue 
{
public:
    explicit RequestQueue(const SearchServer& search_server);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);

    template <class Predicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, Predicate document_predicate)
    {
        auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
        QueueResult(result);
        return result;
    }

    int GetNoResultRequests() const;
private:
    void QueueResult(size_t match_count);
private:
    struct QueryResult
    {
        size_t match_count;
    };
    std::deque<QueryResult> requests_;

    const SearchServer& search_server_;
    
    uint32_t current_min_;
    int zero_result_count_;
    const static int min_in_day_ = 1440;

};
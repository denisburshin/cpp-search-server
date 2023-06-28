#include "request_queue.h"

#include <numeric>

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server), current_min_(0), zero_result_count_(0)
{
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status)
{
    auto result = search_server_.FindTopDocuments(raw_query, status);
    QueueResult(result.size());
    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query)
{
    auto result = search_server_.FindTopDocuments(raw_query);
    QueueResult(result.size());
    return result;
}

int RequestQueue::GetNoResultRequests() const
{
    return std::accumulate(requests_.begin(), requests_.end(), 0,
        [](int count, const QueryResult& result)
        {
            return count += result.match_count == 0;
        });
}

void RequestQueue::QueueResult(size_t match_count)
{
    ++current_min_;

    if (current_min_ > min_in_day_)
    {
        zero_result_count_ -= requests_.front().match_count == 0;
        requests_.pop_front();
    }

    requests_.push_back({ match_count });

    zero_result_count_ += match_count == 0;
}
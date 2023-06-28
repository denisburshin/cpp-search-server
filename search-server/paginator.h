#pragma once
#include <vector>
#include <iostream>
#include <algorithm>

template <class Iterator>
class Page
{
public:
    Page(Iterator begin, Iterator end)
        : begin_(begin), end_(end)
    {}

    inline Iterator begin() const
    {
        return begin_;
    }

    inline Iterator end() const
    {
        return end_;
    }

    inline size_t size() const
    {
        return distance(begin_, end_);
    }

private:
    Iterator begin_, end_;
};

template <class Iterator>
std::ostream& operator<<(std::ostream& out, const Page<Iterator> page)
{
    for (auto doc = page.begin(); doc != page.end(); doc++)
        out << *doc;

    return out;
}

template <class Iterator>
class Paginator
{
public:
    Paginator(Iterator begin, Iterator end, size_t page_size)
        : page_size_(page_size)
    {
        auto it = begin;
        while (it != end)
        {
            auto offset = std::min(page_size, static_cast<size_t>(distance(it, end)));
            advance(it, offset);
            pages_.push_back({ begin, it });
            begin = it;
        }
    }

    inline auto begin() const
    {
        return pages_.begin();
    }

    inline auto end() const
    {
        return pages_.end();
    }

    inline size_t size() const
    {
        return pages_.size();
    }
private:
    std::vector<Page<Iterator>> pages_;
    size_t page_size_;
};

template <class Container>
auto Paginate(const Container& container, size_t page_size)
{
    return Paginator(container.begin(), container.end(), page_size);
}
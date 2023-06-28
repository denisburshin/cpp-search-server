#pragma once
#include <vector>
#include <string>
#include <set>

std::vector<std::string> SplitIntoWords(const std::string& text);

template <class StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) 
{
    std::set<std::string> non_empty_strings;
    for (const std::string& str : strings) 
        if (!str.empty())
            non_empty_strings.insert(str);

    return non_empty_strings;
}
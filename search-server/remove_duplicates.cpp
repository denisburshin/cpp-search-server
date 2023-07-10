#include "remove_duplicates.h"

#include <numeric>
#include <iostream>

void RemoveDuplicates(SearchServer& search_server)
{ 
	const auto& duplicates = FindDuplicates(search_server);

	for (const auto id : duplicates)
		search_server.RemoveDocument(id);
}

std::vector<int> FindDuplicates(SearchServer& search_server)
{
	std::vector<int> duplicates;
	duplicates.reserve(search_server.GetDocumentCount());

	std::set<std::string> unique_text;
	for (const auto document_id : search_server)
	{
		const auto& word_frequence = search_server.GetWordFrequencies(document_id);

		std::vector<std::string> words;
		words.reserve(word_frequence.size());

		size_t document_size = 0;
		for (const auto& [word, _] : word_frequence)
		{
			words.push_back(word);
			document_size += word.size();
		}

		std::string joined_words;
		joined_words.reserve(document_size);

		const auto& text = std::accumulate(words.begin(), words.end(), joined_words);

		if (unique_text.count(text))
		{
			std::cout << "Found duplicate document id " << document_id << std::endl;
			duplicates.push_back(document_id);
		}

		unique_text.insert(text);
	}

	return duplicates;
}

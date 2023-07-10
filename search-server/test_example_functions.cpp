#include "remove_duplicates.h"
#include "test_example_functions.h"

void AddDocument(SearchServer& search_server, int document_id, const std::string& text, DocumentStatus status, const std::vector<int>& ratings)
{
	search_server.AddDocument(document_id, text, status, ratings);
}

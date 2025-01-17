#pragma once
#include "search_server.h"

void AddDocument(SearchServer& search_server, int document_id, const std::string& text,
	DocumentStatus status, const std::vector<int>& ratings);
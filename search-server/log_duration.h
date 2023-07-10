#pragma once
#pragma once

#include <chrono>
#include <iostream>
#include <string>

#define CONCAT_INTERNAL(x, y) x ## y
#define PROFILE_CONCAT(x, y) CONCAT_INTERNAL(x, y)
#define UNIQUE_PROFILER_NAME PROFILE_CONCAT(profiler_guard, __LINE__) 
#define LOG_DURATION(x) LogDuration UNIQUE_PROFILER_NAME(x)
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_PROFILER_NAME("Operation time", y);

class LogDuration
{
public:
	LogDuration(const std::string& operation, std::ostream& out = std::cerr)
		: start_(std::chrono::steady_clock::now()), operation_(operation), stream_(out)
	{ }

	~LogDuration()
	{
		const auto delta = std::chrono::steady_clock::now() - start_;
		stream_ << operation_ << ": " 
			<< std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() << " ms" << std::endl;
	}
private:
	const std::chrono::steady_clock::time_point start_;
	const std::string operation_;
	std::ostream& stream_;
};
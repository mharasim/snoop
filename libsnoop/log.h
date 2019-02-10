/*
MIT License

Copyright (c) 2019 Marcin Harasimczuk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef __LOG_H__
#define __LOG_H__

#include <sstream>
#include <iostream>
#include <string>

namespace logger {

enum class Level { ERROR = 0, WARNING, INFO, DEBUG, MAX};

static Level GetLogLevel() {
	const char* env_level = getenv("LOG_LEVEL");
	if (!env_level)
		return Level::WARNING;
	int level = std::stoi(env_level);
	if (level < 0 || level >= (int)(Level::MAX))
		return Level::WARNING;
	return (Level)(level);
}
static const Level kLevel = GetLogLevel();
static const std::string kSep = " : ";

static std::string ToString(const Level& level) {
	switch(level) {
		case Level::ERROR:
			return "[ERROR]";
		case Level::WARNING:
			return "[WARNING]";
		case Level::INFO:
			return "[INFO]";
		case Level::DEBUG:
			return "[DEBUG]";
		case Level::MAX:
			return "[MAX]";
		default:
			return "Unknown level";
	}
}

class Log {
 public:
	Log(const Level& level, std::ostream& stream = std::cout)
		:	level_(level), stream_(stream)
	{}
	~Log() {
		// @TODO: always flush?
		if (level_ >= kLevel)
			return;
		stream_ << ToString(level_) << kSep << sstream_.rdbuf() << std::endl;
	}
	template<typename Type>
	Log& operator<<(const Type& value) {
		sstream_ << value;
		return *this;
	}
 private:
	Level level_;
	std::ostream& stream_;
	std::stringstream sstream_;
};
#define LOG(level) logger::Log(logger::Level::level)
} //logger
#endif //__LOG_H__

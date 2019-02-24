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

#include <cstdio>
// stoi
#include <string>

namespace logging {

enum Level { ERROR = 0, WARNING, INFO, DEBUG, MAX };

#if defined(SNOOP_LOG_LEVEL)
static const Level kLevel = (Level)(SNOOP_LOG_LEVEL);
#else
static const Level kLevel = ERROR;
#endif

} // namespace logging

#if defined(SNOOP_LOG_DISABLED)
#define SNOOP_LOG_NOLOG
#define LOG(_level_, _format_, ...) SNOOP_LOG_NOLOG
#else
#define PREFIX_ERROR "[ERROR] "
#define PREFIX_WARNING "[WARNING] "
#define PREFIX_INFO "[INFO] "
#define PREFIX_DEBUG "[DEBUG] "
#define LOG(_level_, _format_, ...) \
	if ((int)(logging::_level_) <= (int)(logging::kLevel)) { \
		FILE* _stream_ = logging::_level_ == 0 ? stderr : stdout; \
		std::fprintf(_stream_, PREFIX_##_level_ _format_ "\n", ##__VA_ARGS__); \
	}
#endif

#endif // __LOG_H__

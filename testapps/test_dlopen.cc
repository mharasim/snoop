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
#include <dlfcn.h>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <string.h>

typedef int (*testapi1_t)(int);

int main(int argc, char** argv) {
	std::cout << "-> dlopen\n";
	void* handle = dlopen("./libtest1.so", RTLD_LAZY);
	std::cout << "<- dlopen\n";

	if (!handle) {
		std::cerr << "dlopen failed\n";
		return -1;
	}
	std::cout << "handle=" << (void*)handle << std::endl;

	dlerror();
	testapi1_t testapi1 = (testapi1_t) dlsym(handle, "TestApi1");
	const char* err = dlerror();
	if (err) {
		std::cerr << "dlsym: " << err << std::endl;
		dlclose(handle);
		return -1;
	}

	std::cout << "TestApi1(1) = " << testapi1(1) << std::endl;

	sleep(1);
	dlclose(handle);

	if (argc > 1) {
		std::cout << "Second process exit\n";
		return 0;
	}

	pid_t child = fork();
	if (child == 0) {
		std::string name = "test_dlopen";
		std::string param1 = "param1";
		std::vector<char *> params;
		params.push_back(strdup(name.c_str()));
		params.push_back(strdup(param1.c_str()));
		params.push_back(NULL);
		execv("./test_dlopen", params.data());
	}
	return 0;
}

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
#include <iostream>

int foo1() { return 1; }
int foo2() { return 2; }
int foo3() { return 3; }
int foo4() { return 4; }
int foo5() { return 5; }
int foo6() { return 6; }

int main() {
	std::cout << "after main\n";
	for (int i = 0; i < 500; i++) {
		foo1();
		foo2();
		foo3();
		foo4();
		foo5();
	}
	foo6();
	for (int i = 0; i < 500; i++) {
		foo1();
		foo2();
		foo3();
		foo4();
		foo5();
	}
	foo6();
}

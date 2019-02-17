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
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>
// user_regs_struct
#include <sys/user.h>
#include <stdio.h>
// strerror
#include <string.h>
#ifdef __arm__
#include <asm/ptrace.h>
#include <elf.h>
#endif

#include "log.h"
#include "tracer.h"
#include "snoop.h"

#define RET_ERR 1

// TODO: Investigate PTRACE_O_TRACEFORK

#ifdef __arm__
struct iovec_abi {
	void* handle;
	size_t size;
};
#endif

int do_trace(pid_t pid) {
	LOG(INFO) << "tracer started for PID:" << pid;
	if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
		LOG(ERROR) << "ptrace PTRACE_ATTACH: " << strerror(errno);
		return RET_ERR;
	}
	if (waitpid(pid, 0, 0) == -1) {
		LOG(ERROR) << "waitpid PTRACE_ATTACH: " << strerror(errno);
		return RET_ERR;
	}
	for (;;) {
		// Trap on syscall
		if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
				LOG(ERROR) << "ptrace PTRACE_SYSCALL: " << strerror(errno);
				return RET_ERR;
		}
		if (waitpid(pid, 0, 0) == -1) {
				LOG(ERROR) << "waitpid PTRACE_SYSCALL: " << strerror(errno);
				return RET_ERR;
		}
		// Get syscall arguments
#ifdef __arm__
		struct pt_regs regs;
		struct iovec_abi iov;

		memset(&regs, 0, sizeof(regs));
		iov.handle = (void*)&regs;
		iov.size = sizeof(regs);
		if (ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov) == -1) {
				LOG(ERROR) << "ptrace PTRACE_GETREGSET: " << strerror(errno);
				return RET_ERR;
		}
#else
		struct user_regs_struct regs;
		if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
				LOG(ERROR) << "ptrace PTRACE_GETREGS: " << strerror(errno);
				return RET_ERR;
		}
#endif

#ifdef __arm__
		long syscall = regs.ARM_r7;
#else
		long syscall = regs.orig_rax;
#endif

		// Trap on syscall return
		if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
				LOG(ERROR) << "ptrace PTRACE_SYSCALL (return): " << strerror(errno);
				return RET_ERR;
		}
		if (waitpid(pid, 0, 0) == -1) {
				LOG(ERROR) << "waitpid PTRACE_SYSCALL (return): " << strerror(errno);
				return RET_ERR;
		}
		// Returned from mmap syscall - update process memory map
#ifdef __x86_64__
		long mmap_syscall = SYS_mmap;
#else
		long mmap_syscall = SYS_mmap2;
#endif
		if (syscall == mmap_syscall) {
			snoop::UpdateMemoryMapFile(pid);
		}
	}
}

int main(int argc, char** argv) {
	if (argc < 2) {
		LOG(ERROR) << "expecting pid to trace as 1 argument";
		return RET_ERR;
	}
	pid_t pid = std::stoi(std::string(argv[1]));
	return do_trace(pid);
}

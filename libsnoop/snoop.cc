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
// getpid, gettid
#include <sys/syscall.h>
// open, read, write
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

// exit
#include <unistd.h>
// strdup
#include <string.h>

#include "log.h"
#include "tracer.h"
#include "snoop.h"

namespace {

bool CopyUpdate(const std::string& src, const std::string& dst) {
	static std::vector<std::string> lines = {};
	if (lines.empty()) {
		std::ifstream dst_str_r(dst);
		std::string dst_line;
		while (std::getline(dst_str_r, dst_line))
			lines.push_back(dst_line);
	}
	std::ifstream src_str(src);
	std::ofstream dst_str(dst, std::ios::app | std::ios::out);
	std::string line;
	while (std::getline(src_str, line)) {
		bool exists = false;
		for (auto old_line : lines) {
			if (old_line == line) {
				exists = true;
				break;
			}
		}
		if (!exists) {
			lines.push_back(line);
			dst_str << line << std::endl;
		}
	}
	return true;
}

bool Copy(const char* src, const char* dst) {
	int src_fd = open(src, O_RDONLY);
	if (src_fd < 0) {
		LOG(ERROR) << "Failed to open src file path " << src;
		return false;
	}
	off_t src_size = lseek(src_fd, 0, SEEK_END) + 1;
	if (src_size < 0) {
		LOG(ERROR) << "Failed to get src file size " << src;
		close(src_fd);
		return false;
	}
	if (lseek(src_fd, 0, SEEK_SET) != 0) {
		LOG(ERROR) << "Failed to set src file pos to 0 " << src;
		close(src_fd);
		return false;
	}
	int dst_fd = open(dst, O_RDWR | O_CREAT, 0644);
	if (dst_fd < 0) {
		LOG(ERROR) << "Failed to open dst file path " << dst;
		close(src_fd);
		return false;
	}
	if (posix_fallocate(dst_fd, 0, src_size) < 0) {
		LOG(ERROR) << "Failed to fallocate dst file " << dst;
		close(src_fd);
		close(dst_fd);
		return false;
	}
	char buf[8192];
	while (true) {
		ssize_t num_read = read(src_fd, buf, sizeof(buf));
		if (num_read < 0) {
			if (errno == EINTR)
				continue;
			LOG(ERROR) << "Failed to read src file " << src;
			close(src_fd);
			close(dst_fd);
			return false;
		}
		if (num_read == 0)
			break;
		write(dst_fd, buf, num_read);
	}
	LOG(INFO) << "Copied src " << src << " to dst " << dst;
	close(src_fd);
	close(dst_fd);
	return true;
}

pid_t SpawnTracer(pid_t pid) {
	pid_t tracer_pid = fork();
	if (tracer_pid == 0) {
#ifdef TRACER_USE_EXECVE
		std::string name = "tracer";
		std::string param1 = std::to_string(pid);
		std::vector<char *> params;
		params.push_back(strdup(name.c_str()));
		params.push_back(strdup(param1.c_str()));
		params.push_back(NULL);
		execv("./tracer", params.data());
#else
		do_trace(pid);
		std::exit(EXIT_SUCCESS);
#endif // TRACER_USE_EXECVE
	} else if (tracer_pid > 0) {
		LOG(INFO) << "Tracer process (pid=" << tracer_pid << ") spawned for pid=" << pid;
		return tracer_pid;
	} else {
		LOG(ERROR) << "Fail to spawn tracer process for pid=" << pid;
		return 0;
	}
}

static const std::string kSpace = std::string("_");
static const std::string kExt = std::string(".snoop");
static const std::string kMapExt = std::string(".map");

std::string MemoryMapFileName(pid_t pid) {
	return std::string("/proc/") + std::to_string(pid) + std::string("/maps");
}

/**
 * Reentry of same thread in instrument function will lead to infinite
 * recursion. It means instrumentation of some call that was not
 * supposed to be instrumented
 */
thread_local bool g_tl_reentry_guard = false;

#define LEAVE_ON_REENTRY() \
	if (g_tl_reentry_guard) { \
		return; \
	} else { \
		g_tl_reentry_guard = true; \
	} \

#define LEAVE() \
	if (true) { \
		g_tl_reentry_guard = false; \
	} \

thread_local snoop::ThreadObserver g_tl_observer;

}; // namespace

namespace snoop {

bool DumpMemoryMapFile(pid_t pid) {
	std::string name = std::to_string(pid) + kMapExt;
	if (!Copy(MemoryMapFileName(pid).c_str(), name.c_str())) {
		LOG(ERROR) << "Failed to copy memory map file";
		return false;
	}
	return true;
}

bool UpdateMemoryMapFile(pid_t pid) {
	std::string name = std::to_string(pid) + kMapExt;
	if (!CopyUpdate(MemoryMapFileName(pid), name)) {
		LOG(ERROR) << "Failed to copy memory map file";
		return false;
	}
	return true;
}

StreamingBucketHandler::StreamingBucketHandler(const std::string& name) {
	stream_.open(name, std::ios::out | std::ios::binary | std::ios::app);
}
StreamingBucketHandler::~StreamingBucketHandler() {
	stream_.close();
}

void StreamingBucketHandler::OnMessageBucket(MessageBucket& bucket) {
	if (bucket.empty()) {
		return;
	}
	stream_.write(reinterpret_cast<const char*>(bucket.data()),
								sizeof(uintptr_t) * bucket.size());
	bucket.clear();
}

//static
ThreadManager& ThreadManager::GetInstance() {
	static ThreadManager instance;
	return instance;
}

void ThreadManager::Notify() {
	notify();
}

void ThreadManager::RegisterChannel(std::shared_ptr<Channel> channel) {
	std::lock_guard<std::mutex> lock(internal_state_mutex_);
	LOG(INFO) << "Register channel:" << channel->GetName();
	channels_.push_back(channel);
	const auto name = channel->GetName() + kSpace + std::to_string(pid_) + kExt;
	std::unique_ptr<StreamingBucketHandler> listener(
			new StreamingBucketHandler(name));
	channel->RegisterListener(std::move(listener));
	channel->RegisterConsumer(this);
}

void ThreadManager::UnregisterChannel(std::shared_ptr<Channel> channel) {
	std::lock_guard<std::mutex> lock(internal_state_mutex_);
	LOG(INFO) << "Unregister channel:" << channel->GetName() << " PID: " << getpid();
	auto channel_iterator =
		std::find(channels_.begin(), channels_.end(), channel);
	if (channel_iterator != channels_.end()) {
		(*channel_iterator)->Finalize();
		channels_.erase(channel_iterator);
	}
}

void ThreadManager::ReceiveChannels() {
	std::lock_guard<std::mutex> lock(internal_state_mutex_);
	for (auto channel : channels_) {
		channel->Receive();
	}
}

void ThreadManager::notify() {
	std::lock_guard<std::mutex> lock(processing_mutex_);
	processing_condition_.notify_one();
}

void ThreadManager::close() {
	exit_flag_.store(true, std::memory_order_release);
	notify();
}

bool ThreadManager::should_exit() {
	return exit_flag_.load(std::memory_order_acquire);
}

ThreadManager::ThreadManager() : exit_flag_(false), pid_(getpid()) {
	LOG(INFO) << "Creating thread manager pid: " << pid_;
	SpawnTracer(pid_);
	processing_thread_ = std::thread([this]() {
		LOG(INFO) << "Processing thread started";
		std::unique_lock<std::mutex> lock(processing_mutex_);
		while(true) {
			processing_condition_.wait(lock);
			if (should_exit()) {
				LOG(INFO) << "Processing thread exiting";
				return;
			}
			ReceiveChannels();
		}
	});
}

ThreadManager::~ThreadManager() {
	Deinitialize();
}

void ThreadManager::Deinitialize() {
	std::lock_guard<std::mutex> lock(shutdown_mutex_);
	if (g_exiting)
		return;
	g_exiting = true;
	if (!UpdateMemoryMapFile(pid_))
		LOG(ERROR) << "Failed to dump memory map file";
	LOG(INFO) << "Destroying thread manager";
	close();
	processing_thread_.join();
}

ThreadObserver::ThreadObserver() {
	tid_ = (pid_t)syscall(SYS_gettid);
	LOG(INFO) << "Observe TID:" << tid_ << " PID: " << getpid();
	enter_channel_ = std::make_shared<Channel>(
			constants::kEnterChannelName + kSpace + std::to_string(tid_));
	ThreadManager::GetInstance().RegisterChannel(enter_channel_);
}

ThreadObserver::~ThreadObserver() {
	LOG(INFO) << "Stop observing TID:" << tid_ << " PID:" << getpid();
	exiting_ = true;
	ThreadManager::GetInstance().UnregisterChannel(enter_channel_);
	enter_channel_.reset();
}

void ThreadObserver::Enter(uintptr_t enter_addr) {
	if (exiting_)
		return;
	enter_channel_->Send(enter_addr);
}

} // namespace snoop

extern "C" {
void __cyg_profile_func_enter(void *func,  void *caller) {
	LEAVE_ON_REENTRY();
	if (snoop::g_exiting)
		return;
	g_tl_observer.Enter((uintptr_t)func);
	LEAVE();
}
void __cyg_profile_func_exit(void *func, void *caller) {}
}

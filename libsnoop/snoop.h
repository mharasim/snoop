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
#ifndef __SNOOP_H__
#define __SNOOP_H__

// Linux
#include <sys/types.h> // pid_t

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <thread>
#include <condition_variable>
#include <cstdint>
#include <vector>
#include <atomic>

#include "channel.h"

namespace snoop {

static bool g_exiting = false;

bool DumpMemoryMapFile(pid_t pid);
bool UpdateMemoryMapFile(pid_t pid);

using Channel = Channel<uintptr_t>;
using ChannelListener = Channel::ChannelListener;
using ChannelConsumer = Channel::ChannelConsumer;
using MessageBucket = Channel::MessageBucket;

class StreamingBucketHandler : public ChannelListener {
 public:
	StreamingBucketHandler(const std::string& name);
	~StreamingBucketHandler();
	// ChannelListener
	void OnMessageBucket(MessageBucket& bucket) override;
 private:
	std::ofstream stream_;
};
/*
 * Assuming C++11 and up implies static initialisation thread safety.
 * Meyers Singleton is enough.
 */
class ThreadManager : public ChannelConsumer {
 public:
	// Singleton
	static ThreadManager& GetInstance();
	// ChannelConsumer
	void Notify() override;
	// Interface
	void RegisterChannel(std::shared_ptr<Channel> channel);
	void UnregisterChannel(std::shared_ptr<Channel> channel);
	void ReceiveChannels();

	void Deinitialize();

 protected:
	void notify();
	void close();
	bool should_exit();

 private:
	// Singleton
	ThreadManager();
	ThreadManager(const ThreadManager&) = delete;
	~ThreadManager();

 private:
	std::mutex processing_mutex_;
	std::mutex internal_state_mutex_;
	std::mutex shutdown_mutex_;
	std::condition_variable processing_condition_;
	std::vector<std::shared_ptr<Channel>> channels_;
	std::thread processing_thread_;
	std::atomic_bool exit_flag_;

	pid_t pid_;
};

class ThreadObserver {
public:
	ThreadObserver();
	~ThreadObserver();

	void Enter(uintptr_t enter_addr);

private:
	pid_t tid();

private:
	pid_t tid_;
	std::shared_ptr<Channel> enter_channel_;
	bool exiting_ = false;
};

}; // namespace snoop

#endif //__SNOOP_H__

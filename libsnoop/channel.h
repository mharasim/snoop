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
#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <cstring>

#include <functional>
#include <memory>
#include <vector>

#include "bufferqueue.h"
#include "constants.h"

template<class Message, std::size_t SIZE = constants::kDefaultChannelSize>
class Channel {
 public:
	using MessageBucket = std::vector<Message>;
	class ChannelListener {
	 public:
		virtual ~ChannelListener() {};
		virtual void OnMessageBucket(MessageBucket& bucket) = 0;
	};
	class ChannelConsumer {
	 public:
		virtual void Notify() = 0;
	};
	Channel()
		: consumer_(nullptr)
	{}
	~Channel() {
		listeners_.clear();
	}
	bool SetName(const char* name, pid_t pid) {
		const auto ret = std::snprintf(name_, constants::kNameSizeMax, "%s_%d", name, pid);
		if (ret < 0 || ret >= constants::kNameSizeMax)
			return false;
		return true;
	}
	bool Send(const Message& message) {
		MessageBucket* bucket = queue_.Get();
		if (!bucket) {
			drop_count_++;
			return false;
		}
		bucket->push_back(message);
		if (bucket->size() == constants::kDefaultChannelBucketSize) {
			queue_.Push();
			maybe_notify_consumer();
		}
		return true;
	}
	void RegisterListener(std::unique_ptr<ChannelListener> listener) {
		listeners_.push_back(std::move(listener));
	}
	void RegisterConsumer(ChannelConsumer* consumer) {
		consumer_ = consumer;
	}
	void NotifyMessageBucket(MessageBucket& bucket) {
		for (auto& listener : listeners_)
			listener->OnMessageBucket(bucket);
	}
	void Receive() {
		queue_.Process(std::bind(&Channel::NotifyMessageBucket, this,
					std::placeholders::_1));
	}
	void Finalize() {
		queue_.Consume(std::bind(&Channel::NotifyMessageBucket, this,
					std::placeholders::_1));
	}
	const char* GetName() const {
		return name_;
	}
 protected:
	void maybe_notify_consumer() {
		if (consumer_)
			consumer_->Notify();
	}
 private:
	BufferQueue<MessageBucket, SIZE> queue_;
	std::vector<std::unique_ptr<ChannelListener>> listeners_;
	ChannelConsumer* consumer_;
	long drop_count_;
	char name_[constants::kNameSizeMax];
};

#endif // __CHANNEL_H__

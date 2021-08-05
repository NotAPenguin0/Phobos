#pragma once

#include <vector>

namespace ph {

template<typename T>
class RingBuffer {
public:
	using container = std::vector<T>;
	using iterator = typename container::iterator;

	RingBuffer() = default;
	RingBuffer(size_t elem_count) {
		buffer.resize(elem_count);
	}

	RingBuffer(RingBuffer const& rhs) = default;
	RingBuffer(RingBuffer&& rhs) = default;

	RingBuffer& operator=(RingBuffer const& rhs) = default;
	RingBuffer& operator=(RingBuffer&& rhs) = default;

	iterator begin() { return buffer.begin(); }
	iterator end() { return buffer.end(); }

	size_t size() const {
		return buffer.size();
	}

	void next() {
		++cur;
		cur %= size();
	}

	size_t index() const {
		return cur;
	}

	T& current() {
		return buffer[cur];
	}

	T& previous() {
		if (cur == 0) return buffer.back();
		else return buffer[cur - 1];
	}

	void set(size_t index, T&& value) {
		buffer[index] = std::move(value);
	}

private:
	container buffer{};
	size_t cur = 0;
};

}
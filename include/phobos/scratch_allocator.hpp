#pragma once 

#include <phobos/buffer.hpp>

namespace ph {

class Context;

class ScratchAllocator {
public:
	ScratchAllocator() = default;
	ScratchAllocator(Context* ctx, VkDeviceSize size, VkDeviceSize alignment, BufferType type);
	ScratchAllocator(ScratchAllocator&& rhs) noexcept;

	~ScratchAllocator();

	ScratchAllocator& operator=(ScratchAllocator&& rhs) noexcept;

	BufferSlice allocate(VkDeviceSize size);

	// Cleans up state of the allocator. This resets the current offset to 0 and thus frees allocator memory
	void reset();
private:
	Context* ctx = nullptr;

	RawBuffer buffer{};
	// alignment in bytes between subsequent allocations
	VkDeviceSize alignment = 0;
	VkDeviceSize current_offset = 0;
	std::byte* base_pointer = nullptr;
};

}
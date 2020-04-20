#ifndef PHOBOS_BUFFER_ALLOCATOR_HPP_
#define PHOBOS_BUFFER_ALLOCATOR_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/util/buffer_util.hpp>

namespace ph {

class BufferAllocator {
public:
	BufferAllocator() = default;
	BufferAllocator(VulkanContext* ctx, vk::DeviceSize size, vk::DeviceSize alignment, BufferType type);
	BufferAllocator(BufferAllocator&&);

	~BufferAllocator();

	BufferAllocator& operator=(BufferAllocator&&);

	BufferSlice allocate(vk::DeviceSize size);

	// Cleans up state of the allocator. This resets the current offset to 0 and thus frees allocator memory
	void reset();

	void destroy();
private:
	VulkanContext* ctx = nullptr;

	RawBuffer buffer{};
	// alignment in bytes between subsequent allocations
	vk::DeviceSize alignment = 0;
	vk::DeviceSize current_offset = 0;
	std::byte* base_pointer = nullptr;
};

}

#endif
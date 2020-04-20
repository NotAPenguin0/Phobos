#include <phobos/memory/buffer_allocator.hpp>

namespace ph {

BufferAllocator::BufferAllocator(VulkanContext* ctx, vk::DeviceSize size, vk::DeviceSize alignment, BufferType type) 
	: ctx(ctx), alignment(alignment) {
	buffer = create_buffer(*ctx, size, type);
	
	if (!has_persistent_mapping(buffer)) {
		ctx->logger->write_fmt(log::Severity::Error, "Buffer allocator created with buffer type {}, but that buffer type does not support persistent mapping");
		return;
	}

	base_pointer = map_memory(*ctx, buffer);
}

BufferAllocator::BufferAllocator(BufferAllocator&& rhs) {
	ctx = rhs.ctx;
	buffer = rhs.buffer;
	current_offset = rhs.current_offset;
	alignment = rhs.alignment;
	base_pointer = rhs.base_pointer;

	rhs.ctx = nullptr;
	rhs.buffer = {};
	rhs.current_offset = 0;
	rhs.alignment = 0;
	rhs.base_pointer = nullptr;
}

BufferAllocator& BufferAllocator::operator=(BufferAllocator&& rhs) {
	if (this != &rhs) {
		ctx = rhs.ctx;
		buffer = rhs.buffer;
		current_offset = rhs.current_offset;
		alignment = rhs.alignment;
		base_pointer = rhs.base_pointer;

		rhs.ctx = nullptr;
		rhs.buffer = {};
		rhs.current_offset = 0;
		rhs.alignment = 0;
		rhs.base_pointer = nullptr;
	}
	return *this;
}

BufferAllocator::~BufferAllocator() {
	destroy();
}

BufferSlice BufferAllocator::allocate(vk::DeviceSize size) {
	STL_ASSERT(is_valid_buffer(buffer), "Buffer allocator does not hold a valid buffer");
	STL_ASSERT(base_pointer, "Base pointer does not point to mapped buffer memory");

	vk::DeviceSize const unaligned_size = size % alignment;
	vk::DeviceSize const padding = alignment - unaligned_size;

	vk::DeviceSize const aligned_size = size + padding;

	vk::DeviceSize const offset = current_offset;
	STL_ASSERT(offset + aligned_size <= buffer.size, "Buffer allocator out of memory");

	current_offset += aligned_size;
	
	std::byte* data = base_pointer + offset;
	return BufferSlice{ buffer.buffer, offset, size, data };
}

void BufferAllocator::reset() {
	current_offset = 0;
}

void BufferAllocator::destroy() {
	reset();
	if (is_valid_buffer(buffer)) {
		destroy_buffer(*ctx, buffer);
		base_pointer = nullptr;
	}
}

}
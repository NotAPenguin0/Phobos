#include <phobos/scratch_allocator.hpp>
#include <phobos/context.hpp>

#include <cassert>

namespace ph {

ScratchAllocator::ScratchAllocator(Context* ctx, VkDeviceSize size, VkDeviceSize alignment, BufferType type)
	: ctx(ctx), alignment(alignment) {

	buffer = ctx->create_buffer(type, size);

	assert(ctx->has_persistent_mapping(buffer) && "Scratch allocator buffer must be persistently mappable");

	base_pointer = ctx->map_memory(buffer);
}

ScratchAllocator::ScratchAllocator(ScratchAllocator&& rhs) noexcept {
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

ScratchAllocator& ScratchAllocator::operator=(ScratchAllocator&& rhs) noexcept {
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

ScratchAllocator::~ScratchAllocator() {
	reset();
	if (!ctx) return;
	if (ctx->is_valid_buffer(buffer)) {
		ctx->destroy_buffer(buffer);
		base_pointer = nullptr;
	}
}

BufferSlice ScratchAllocator::allocate(VkDeviceSize size) {
	assert(ctx->is_valid_buffer(buffer) && "Buffer allocator does not hold a valid buffer");
	assert(base_pointer && "Base pointer does not point to mapped buffer memory");

	VkDeviceSize const unaligned_size = size % alignment;
	VkDeviceSize const padding = alignment - unaligned_size;

	VkDeviceSize const aligned_size = size + padding;

	VkDeviceSize const offset = current_offset;
	assert(offset + aligned_size <= buffer.size && "Buffer allocator out of memory");

	current_offset += aligned_size;

	std::byte* data = base_pointer + offset;
	return BufferSlice(buffer, size, offset, data);
}

void ScratchAllocator::reset() {
	current_offset = 0;
}

RawBuffer const& ScratchAllocator::get_buffer() const {
	return buffer;
}


}
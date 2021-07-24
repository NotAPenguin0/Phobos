#pragma once

#include <phobos/context.hpp>


namespace ph {
namespace impl {

class BufferImpl {
public:
	BufferImpl(ContextImpl& ctx);

	RawBuffer create_buffer(BufferType type, VkDeviceSize size);
	void destroy_buffer(RawBuffer& buffer);

	bool is_valid_buffer(RawBuffer const& buffer);
	bool has_persistent_mapping(RawBuffer const& buffer);

	// For a persistently mapped buffer, this does not remap memory, and instead returns the pointer immediately.
	// Persistently mapped buffers are buffers with buffer type MappedUniformBuffer and StorageBufferDynamic
	std::byte* map_memory(RawBuffer& buffer);
	// Flushes memory owned by the buffer passed in. For memory that is not host-coherent, this is required to make
	// changes visible to the gpu after writing to mapped memory. If you want to flush the whole mapped range, size can be VK_WHOLE_SIZE
	void flush_memory(BufferSlice slice);
	void unmap_memory(RawBuffer& buffer);

	// Resizes the raw buffer to be able to hold at least requested_size bytes. Returns whether a reallocation occured. 
	// Note: If the buffer needs a resize, the old contents will be lost.
	bool ensure_buffer_size(RawBuffer& buf, VkDeviceSize requested_size);

	// Returns the device address for a buffer
	VkDeviceAddress get_device_address(RawBuffer const& buf);
	VkDeviceAddress get_device_address(BufferSlice slice);
private:
	ContextImpl* ctx;
};

}
}
#include <phobos/buffer.hpp>

namespace ph {

BufferSlice RawBuffer::slice(VkDeviceSize offset, VkDeviceSize range) {
	BufferSlice slice{};
	slice.buffer = handle;
	slice.memory = memory;
	slice.offset = offset;
	slice.range = range;
	return slice;
}

}
#include <phobos/buffer.hpp>

namespace ph {

BufferSlice::BufferSlice(RawBuffer& buffer, VkDeviceSize range, VkDeviceSize offset)
	: buffer(buffer.handle), memory(buffer.memory), offset(offset), data(nullptr){

	if (range == WHOLE_BUFFER_SIZE) { this->range = buffer.size; }
	else { this->range = range; }
}

}
#include <phobos/renderer/dynamic_gpu_buffer.hpp>

namespace ph {

DynamicGpuBuffer::DynamicGpuBuffer(VulkanContext& ctx) : ctx(&ctx) {

}

void DynamicGpuBuffer::create(size_t initial_size, size_t) {
    buffer = create_buffer(*ctx, initial_size, BufferType::StorageBufferDynamic);
    // Note that this is a cheap operation
    data_ptr = map_memory(*ctx, buffer);
}

void DynamicGpuBuffer::destroy() {
    if (buffer.buffer) {
        destroy_buffer(*ctx, buffer);
    }
}

vk::Buffer DynamicGpuBuffer::buffer_handle() {
    return buffer.buffer;
}

VmaAllocation DynamicGpuBuffer::memory_handle() {
    return buffer.memory;
}

size_t DynamicGpuBuffer::size() const {
    return buffer.size;
}

void DynamicGpuBuffer::write_data(vk::DescriptorSet descriptor_set, void const* data, size_t size, size_t offset) {
    if (offset + size > buffer.size) {
        size_t new_size = buffer.size;
        while(offset + size > new_size) {
            new_size *= 2;
        }
        destroy();
        create(new_size);
        ctx->event_dispatcher.fire_event(DynamicGpuBufferResizeEvent{buffer.buffer, descriptor_set, new_size});
    }

    std::memcpy(data_ptr + offset, data, size);
}


}
#include <phobos/renderer/dynamic_gpu_buffer.hpp>

#include <phobos/util/buffer_util.hpp>

namespace ph {

DynamicGpuBuffer::DynamicGpuBuffer(VulkanContext& ctx) : ctx(&ctx) {

}

void DynamicGpuBuffer::create(size_t initial_size, size_t) {
    current_size = initial_size;
    create_buffer(*ctx, initial_size, vk::BufferUsageFlagBits::eStorageBuffer, 
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, 
        buffer, memory);
    data_ptr = ctx->device.mapMemory(memory, 0, initial_size); 
}

void DynamicGpuBuffer::destroy() {
    if (buffer) {
        ctx->device.destroyBuffer(buffer);
        buffer = nullptr;
    }

    if (memory) {
        if (data_ptr) {
            ctx->device.unmapMemory(memory);
            data_ptr = nullptr;
        }
        ctx->device.freeMemory(memory);
        current_size = 0;
        memory = nullptr;
    }
}

vk::Buffer DynamicGpuBuffer::buffer_handle() {
    return buffer;
}

vk::DeviceMemory DynamicGpuBuffer::memory_handle() {
    return memory;
}

size_t DynamicGpuBuffer::size() const {
    return current_size;
}

void DynamicGpuBuffer::write_data(vk::DescriptorSet descriptor_set, void const* data, size_t size, size_t offset) {
    if (offset + size > current_size) {
        size_t new_size = current_size;
        while(offset + size > new_size) {
            new_size *= 2;
        }
        destroy();
        create(new_size);
        ctx->event_dispatcher.fire_event(DynamicGpuBufferResizeEvent{buffer, descriptor_set, new_size});
    }

    std::memcpy((unsigned char*)data_ptr + offset, data, size);
}


}
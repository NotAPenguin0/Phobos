#include <phobos/renderer/instancing_buffer.hpp>

#include <phobos/util/buffer_util.hpp>

namespace ph {

InstancingBuffer::InstancingBuffer(VulkanContext& ctx) : ctx(&ctx) {

}

void InstancingBuffer::create(size_t initial_size, size_t) {
    current_size = initial_size;
    create_buffer(*ctx, initial_size, vk::BufferUsageFlagBits::eStorageBuffer, 
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, 
        buffer, memory);
    ctx->device.bindBufferMemory(buffer, memory, 0);
    data_ptr = ctx->device.mapMemory(memory, 0, initial_size); 
}

void InstancingBuffer::destroy() {
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

vk::Buffer InstancingBuffer::buffer_handle() {
    return buffer;
}

vk::DeviceMemory InstancingBuffer::memory_handle() {
    return memory;
}

size_t InstancingBuffer::size() const {
    return current_size;
}

void InstancingBuffer::write_data(void const* data, size_t size) {
    was_resized = false;
    if (size > current_size) {
        size_t new_size = current_size;
        while(size > new_size) {
            new_size *= 2;
        }
        destroy();
        create(new_size);
        was_resized = true;
    }

    std::memcpy(data_ptr, data, size);
}

bool InstancingBuffer::last_write_resized() const {
    return was_resized;
}

}
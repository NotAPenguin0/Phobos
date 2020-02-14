#include <phobos/renderer/mesh.hpp>

#include <phobos/util/memory_util.hpp>
#include <phobos/util/buffer_util.hpp>

namespace ph {

Mesh::Mesh(VulkanContext& ctx) : ctx(&ctx) {
    
}

Mesh::Mesh(CreateInfo const& info) : Mesh(*info.ctx) {
    create(info);
}   

Mesh::Mesh(Mesh&& rhs) : Mesh(*rhs.ctx) {
    std::swap(buffer, rhs.buffer);
    std::swap(memory, rhs.memory);
}

Mesh& Mesh::operator=(Mesh&& rhs) {
    if (this != &rhs) {
        std::swap(ctx, rhs.ctx);
        std::swap(buffer, rhs.buffer);
        std::swap(memory, rhs.memory);
    }
    return *this;
}

Mesh::~Mesh() {
    destroy();
}

void Mesh::create(CreateInfo const& info) {
    destroy();

    create_vertex_buffer(info);
    create_index_buffer(info);
}

void Mesh::destroy() {
    if (buffer) {
        ctx->device.destroyBuffer(buffer);
        buffer = nullptr;
    }

    if (index_buffer) {
        ctx->device.destroyBuffer(index_buffer);
        index_buffer = nullptr;
    }

    if (index_buffer_memory) {
        ctx->device.freeMemory(index_buffer_memory);
        index_buffer_memory = nullptr;
    }

    if (memory) {
        ctx->device.freeMemory(memory);
        memory = nullptr;
    }
}

vk::Buffer Mesh::get_vertices() {
    return buffer;
}

vk::Buffer Mesh::get_indices() {
    return index_buffer;
}

size_t Mesh::get_vertex_count() const {
    return vertex_count;
}

size_t Mesh::get_index_count() const {
    return index_count;
}

vk::DeviceMemory Mesh::get_memory_handle() {
    return memory;
}

vk::DeviceMemory Mesh::get_index_memory() {
    return index_buffer_memory;
}

void Mesh::create_vertex_buffer(CreateInfo const& info) {
    vertex_count = info.vertex_count;
    vk::DeviceSize byte_size = info.vertex_count * info.vertex_size * sizeof(float);

    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_buffer_memory;

    create_buffer(*ctx, byte_size, vk::BufferUsageFlagBits::eTransferSrc, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, staging_buffer, staging_buffer_memory);

    // Fill staging buffer
    ctx->device.bindBufferMemory(staging_buffer, staging_buffer_memory, 0);
    void* data_ptr = ctx->device.mapMemory(staging_buffer_memory, 0, byte_size);
    std::memcpy(data_ptr, info.vertices, byte_size);
    ctx->device.unmapMemory(staging_buffer_memory);

    // Now copy this buffer to device local memory
    create_buffer(*ctx, byte_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal, buffer, memory);
    ctx->device.bindBufferMemory(buffer, memory, 0);
    copy_buffer(*ctx, staging_buffer, buffer, byte_size);

    // Free the staging buffer
    ctx->device.destroyBuffer(staging_buffer);
    ctx->device.freeMemory(staging_buffer_memory);
}

void Mesh::create_index_buffer(CreateInfo const& info) {
    index_count = info.index_count;
    vk::DeviceSize byte_size = info.index_count * sizeof(uint32_t);

    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_buffer_memory;
    create_buffer(*ctx, byte_size, vk::BufferUsageFlagBits::eTransferSrc, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, staging_buffer, staging_buffer_memory);
    // Fill staging buffer
    ctx->device.bindBufferMemory(staging_buffer, staging_buffer_memory, 0);
    void* data_ptr = ctx->device.mapMemory(staging_buffer_memory, 0, byte_size);
    std::memcpy(data_ptr, info.indices, byte_size);
    ctx->device.unmapMemory(staging_buffer_memory);
    // Now copy this buffer to device local memory
    create_buffer(*ctx, byte_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal, index_buffer, index_buffer_memory);
    ctx->device.bindBufferMemory(index_buffer, index_buffer_memory, 0);
    copy_buffer(*ctx, staging_buffer, index_buffer, byte_size);

    // Free the staging buffer
    ctx->device.destroyBuffer(staging_buffer);
    ctx->device.freeMemory(staging_buffer_memory);
}

} // namespace ph
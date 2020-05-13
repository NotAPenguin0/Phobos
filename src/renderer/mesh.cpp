#include <phobos/renderer/mesh.hpp>

#include <phobos/util/memory_util.hpp>
#include <phobos/util/buffer_util.hpp>
#include <phobos/core/vulkan_context.hpp>

namespace ph {

Mesh::Mesh(VulkanContext& ctx) : ctx(&ctx) {
    
}

Mesh::Mesh(CreateInfo const& info) : Mesh(*info.ctx) {
    create(info);
}   

Mesh::Mesh(Mesh&& rhs) : Mesh(*rhs.ctx) {
    std::swap(ctx, rhs.ctx);
    std::swap(vertex_buffer, rhs.vertex_buffer);
    std::swap(index_buffer, rhs.index_buffer);
    std::swap(vertex_count, rhs.vertex_count);
    std::swap(index_count, rhs.index_count);
}

Mesh& Mesh::operator=(Mesh&& rhs) {
    if (this != &rhs) {
        std::swap(ctx, rhs.ctx);
        std::swap(vertex_buffer, rhs.vertex_buffer);
        std::swap(index_buffer, rhs.index_buffer);
        std::swap(vertex_count, rhs.vertex_count);
        std::swap(index_count, rhs.index_count);
    }
    return *this;
}

Mesh::~Mesh() {
    destroy();
}

void Mesh::create(CreateInfo const& info) {
    destroy();
    ctx = info.ctx;
    create_vertex_buffer(info);
    create_index_buffer(info);
}

void Mesh::destroy() {
    if (vertex_buffer.buffer) {
        destroy_buffer(*ctx, vertex_buffer);
    }
    if (index_buffer.buffer) {
        destroy_buffer(*ctx, index_buffer);
    }
}

vk::Buffer Mesh::get_vertices() const {
    return vertex_buffer.buffer;
}

vk::Buffer Mesh::get_indices() const {
    return index_buffer.buffer;
}

size_t Mesh::get_vertex_count() const {
    return vertex_count;
}

size_t Mesh::get_index_count() const {
    return index_count;
}

VmaAllocation Mesh::get_memory_handle() const {
    return vertex_buffer.memory;
}

VmaAllocation Mesh::get_index_memory() const {
    return index_buffer.memory;
}

void Mesh::create_vertex_buffer(CreateInfo const& info) {
    vertex_count = info.vertex_count;
    vk::DeviceSize byte_size = info.vertex_count * info.vertex_size * sizeof(float);

    RawBuffer staging_buffer = create_buffer(*ctx, byte_size, BufferType::TransferBuffer);

    // Fill staging buffer
    std::byte* data_ptr = map_memory(*ctx, staging_buffer);
    std::memcpy(data_ptr, info.vertices, byte_size);
    flush_memory(*ctx, staging_buffer, 0, byte_size);
    unmap_memory(*ctx, staging_buffer);

    // Now copy this buffer to device local memory
    vertex_buffer = create_buffer(*ctx, byte_size, BufferType::VertexBuffer);
    copy_buffer(*ctx, staging_buffer, vertex_buffer, byte_size);

    destroy_buffer(*ctx, staging_buffer);
}

void Mesh::create_index_buffer(CreateInfo const& info) {
    index_count = info.index_count;
    vk::DeviceSize byte_size = info.index_count * sizeof(uint32_t);

    RawBuffer staging_buffer = create_buffer(*ctx, byte_size, BufferType::TransferBuffer);

    // Fill staging buffer
    std::byte* data_ptr = map_memory(*ctx, staging_buffer);
    std::memcpy(data_ptr, info.indices, byte_size);
    flush_memory(*ctx, staging_buffer, 0, byte_size);
    unmap_memory(*ctx, staging_buffer);

    // Now copy this buffer to device local memory
    index_buffer = create_buffer(*ctx, byte_size, BufferType::IndexBuffer);
    copy_buffer(*ctx, staging_buffer, index_buffer, byte_size);

    destroy_buffer(*ctx, staging_buffer);
}

} // namespace ph
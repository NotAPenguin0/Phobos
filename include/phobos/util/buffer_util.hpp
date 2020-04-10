#ifndef PHOBOS_BUFFER_UTIL_HPP_
#define PHOBOS_BUFFER_UTIL_HPP_

#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>
#include <cstddef>

namespace ph {

struct VulkanContext;

enum class BufferType {
    Undefined,
    TransferBuffer,
    VertexBuffer,
    VertexBufferDynamic,
    // Frequently updating SSBO
    StorageBufferDynamic,
    // Not frequently updating SSBO
    StorageBufferStatic,
    MappedUniformBuffer,
    IndexBuffer,
    IndexBufferDynamic
};

struct RawBuffer {
    BufferType type = BufferType::Undefined;
    vk::DeviceSize size = 0;
    vk::Buffer buffer = nullptr;
    VmaAllocation memory = nullptr;
};

RawBuffer create_buffer(VulkanContext& ctx, vk::DeviceSize size, BufferType buf_type);
void destroy_buffer(VulkanContext& ctx, RawBuffer& buffer);

bool is_valid_buffer(RawBuffer const& buffer);

// For a persistently mapped buffer, this does not remap memory, and instead returns the pointer immediately.
// Persistently mapped buffers are buffers with BufferType MappedUniformBuffer and StorageBufferDynamic
std::byte* map_memory(VulkanContext& ctx, RawBuffer& buffer);
// Flushes memory owned by the buffer passed in. For memory that is not host-coherent, this is required to make
// changes visible to the gpu after writing to mapped memory. If you want to flush the whole mapped range, size can be VK_WHOLE_SIZE
void flush_memory(VulkanContext& ctx, RawBuffer& buffer, vk::DeviceSize offset, vk::DeviceSize size);
void unmap_memory(VulkanContext& ctx, RawBuffer& buffer);

// Resizes the raw buffer to be able to hold at least requested_size bytes. Returns whether a reallocation occured. 
// Note: If the buffer needs a resize, the old contents will be lost.
bool ensure_buffer_size(VulkanContext& ctx, RawBuffer& buf, vk::DeviceSize requested_size);

void copy_buffer(VulkanContext& ctx, RawBuffer const& src, RawBuffer& dst, vk::DeviceSize size);

}

#endif
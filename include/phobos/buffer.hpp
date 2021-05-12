#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstddef>

namespace ph {

// Alias for VK_WHOLE_SIZE
static constexpr VkDeviceSize WHOLE_BUFFER_SIZE = VK_WHOLE_SIZE;

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
    VkDeviceSize size = 0;
    VkBuffer handle = nullptr;
    VmaAllocation memory = nullptr;
};

struct BufferSlice {
    VkBuffer buffer = nullptr;
    VkDeviceSize offset = 0;
    VkDeviceSize range = 0;

    VmaAllocation memory = nullptr;

    // Only present on buffers that are mappable
    std::byte* data = nullptr;

    BufferSlice() = default;
    BufferSlice(RawBuffer& buffer, VkDeviceSize range = WHOLE_BUFFER_SIZE, VkDeviceSize offset = 0);
    BufferSlice(BufferSlice const&) = default;
    BufferSlice& operator=(BufferSlice const&) = default;
};

}
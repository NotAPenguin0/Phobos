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

// Note that free conversions from BufferSlice to TypedBufferSlice and vice versa are allowed.

template<typename T>
struct TypedBufferSlice;

using BufferSlice = TypedBufferSlice<std::byte>;

template<typename T>
struct TypedBufferSlice {
    VkBuffer buffer = nullptr;
    VkDeviceSize offset = 0;
    VkDeviceSize range = 0;
    VmaAllocation memory = nullptr;

    T* data = nullptr;

    TypedBufferSlice() = default;
    TypedBufferSlice(RawBuffer& buffer, VkDeviceSize range = WHOLE_BUFFER_SIZE, VkDeviceSize offset = 0, T* data = nullptr) 
        : buffer(buffer.handle), memory(buffer.memory), offset(offset), data(data) {

        if (range == WHOLE_BUFFER_SIZE) { this->range = buffer.size; }
        else { this->range = range; }
    }

    // Conversion from BufferSlice to TypedBufferSlice.
    // Note that the enable_if is necessary to make sure the constructor isn't defined twice when T is std::byte
    template<typename = std::enable_if<!std::is_same_v<T, std::byte>, void>>
    TypedBufferSlice(BufferSlice const& slice) {
        buffer = slice.buffer;
        offset = slice.offset;
        range = slice.range;
        memory = slice.memory;
        data = reinterpret_cast<T*>(slice.data);
    }

    TypedBufferSlice(TypedBufferSlice const&) = default;
    TypedBufferSlice& operator=(TypedBufferSlice const&) = default;
    
    // Conversion from TypedbufferSlice to BufferSlice
    operator BufferSlice() {
        // This isn't working for some reason, guess we'll manually assign the values ...
        // return TypedBufferSlice<std::byte>{ .buffer = buffer, .offset = offset, .range = range, .memory = memory, .data = reinterpret_cast<std::byte*>(data) };
        BufferSlice slice;
        slice.buffer = buffer;
        slice.offset = offset;
        slice.range = range;
        slice.memory = memory;
        slice.data = reinterpret_cast<std::byte*>(data);
        return slice;
    }
};

}
// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Buffer creation and management

#include <phobos/impl/buffer.hpp>
#include <phobos/impl/context.hpp>

namespace ph {
namespace impl {

static VkBufferUsageFlags get_usage_flags(BufferType buf_type) {
    switch (buf_type) {
    case BufferType::TransferBuffer:
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case BufferType::VertexBuffer:
    case BufferType::VertexBufferDynamic:
        return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
#if PHOBOS_ENABLE_RAY_TRACING
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
#endif
    case BufferType::StorageBufferDynamic:
    case BufferType::StorageBufferStatic:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case BufferType::MappedUniformBuffer:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case BufferType::IndexBuffer:
    case BufferType::IndexBufferDynamic:
        return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
#if PHOBOS_ENABLE_RAY_TRACING
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
#endif
    case BufferType::AccelerationStructure:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR 
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case BufferType::AccelerationStructureScratch:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case BufferType::AccelerationStructureInstance:
        return VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case BufferType::Undefined:
        return VkBufferUsageFlags{};
    }
}

static VmaMemoryUsage get_memory_usage(BufferType buf_type) {
    switch (buf_type) {
    case BufferType::TransferBuffer:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY;
    case BufferType::VertexBuffer:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    case BufferType::VertexBufferDynamic:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
    case BufferType::IndexBuffer:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    case BufferType::IndexBufferDynamic:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
    case BufferType::StorageBufferDynamic:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
    case BufferType::StorageBufferStatic:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    case BufferType::MappedUniformBuffer:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
    case BufferType::AccelerationStructure:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    case BufferType::AccelerationStructureScratch:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    case BufferType::AccelerationStructureInstance:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    case BufferType::Undefined:
        return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    }
}

static VmaAllocationCreateFlags get_allocation_flags(BufferType buf_type) {
    switch (buf_type) {
    case BufferType::MappedUniformBuffer: return VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
    case BufferType::StorageBufferDynamic: return VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
    case BufferType::IndexBufferDynamic: return VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
    case BufferType::VertexBufferDynamic: return VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
    default:
        return {};
    }
}

BufferImpl::BufferImpl(ContextImpl& ctx) : ctx(&ctx) {

}

RawBuffer BufferImpl::create_buffer(BufferType type, VkDeviceSize size) {
    if (type == BufferType::Undefined) {
        return {};
    }

    if (size == 0) {
        return {};
    }

    RawBuffer buffer;
    buffer.size = size;
    buffer.type = type;

    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = get_usage_flags(type);
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = get_memory_usage(type);
    alloc_info.flags = get_allocation_flags(type) | VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    vmaCreateBuffer(ctx->allocator, &info, &alloc_info, &buffer.handle, &buffer.memory, nullptr);

    return buffer;
}

void BufferImpl::destroy_buffer(RawBuffer& buffer) {
    if (!is_valid_buffer(buffer)) {
        return;
    }

    vmaDestroyBuffer(ctx->allocator, buffer.handle, buffer.memory);
    buffer.size = 0;
    buffer.handle = nullptr;
    buffer.memory = nullptr;
}

bool BufferImpl::is_valid_buffer(RawBuffer const& buffer) {
    return buffer.type != BufferType::Undefined && buffer.size > 0 && buffer.handle && buffer.memory;
}

bool BufferImpl::has_persistent_mapping(RawBuffer const& buffer) {
    return get_allocation_flags(buffer.type) & VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
}

std::byte* BufferImpl::map_memory(RawBuffer& buffer) {
    if (!is_valid_buffer(buffer)) {
        return nullptr;
    }

    if (has_persistent_mapping(buffer)) {
        VmaAllocationInfo alloc_info;
        vmaGetAllocationInfo(ctx->allocator, buffer.memory, &alloc_info);
        return static_cast<std::byte*>(alloc_info.pMappedData);
    }

    void* ptr;
    vmaMapMemory(ctx->allocator, buffer.memory, &ptr);
    return static_cast<std::byte*>(ptr);
}


void BufferImpl::flush_memory(BufferSlice slice) {
    if (!slice.buffer || !slice.memory) {
        return;
    }

    vmaFlushAllocation(ctx->allocator, slice.memory, slice.offset, slice.range);
}

void BufferImpl::unmap_memory(RawBuffer& buffer) {
    if (!is_valid_buffer(buffer)) {
        return;
    }

    // Don't unmap persistently mapped buffer
    if (has_persistent_mapping(buffer)) {
        return;
    }

    vmaUnmapMemory(ctx->allocator, buffer.memory);
}

bool BufferImpl::ensure_buffer_size(RawBuffer& buf, VkDeviceSize requested_size) {
    if (!is_valid_buffer(buf)) {
        return false;
    }

    if (buf.size >= requested_size) { return false; }

    // Save buffer type as it is reset to Undefined after destroy_buffer
    BufferType buf_type = buf.type;
    destroy_buffer(buf);
    buf = create_buffer(buf_type, requested_size);

    return true;
}

VkDeviceAddress BufferImpl::get_device_address(RawBuffer const& buf) {
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buf.handle
    };
    return vkGetBufferDeviceAddress(ctx->device, &info);
}

VkDeviceAddress BufferImpl::get_device_address(BufferSlice slice) {
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = slice.buffer
    };
    // Since a VkDeviceAddress is simply an address we can offset it by the slice offset
    // to obtain the device address to a range in a buffer.
    return vkGetBufferDeviceAddress(ctx->device, &info) + slice.offset;
}

}
}
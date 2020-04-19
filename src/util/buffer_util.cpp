#include <phobos/util/buffer_util.hpp>
#include <phobos/util/memory_util.hpp>

#include <phobos/core/vulkan_context.hpp>

namespace ph {

static vk::BufferUsageFlags get_usage_flags(BufferType buf_type) {
    switch (buf_type) {
        case BufferType::TransferBuffer: 
            return vk::BufferUsageFlagBits::eTransferSrc;
        case BufferType::VertexBuffer: 
        case BufferType::VertexBufferDynamic:
            return vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        case BufferType::StorageBufferDynamic: 
        case BufferType::StorageBufferStatic:
            return vk::BufferUsageFlagBits::eStorageBuffer;
        case BufferType::MappedUniformBuffer: 
            return vk::BufferUsageFlagBits::eUniformBuffer;
        case BufferType::IndexBuffer: 
        case BufferType::IndexBufferDynamic:
            return vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
        case BufferType::Undefined:
            return vk::BufferUsageFlags{};
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
        case BufferType::Undefined:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    }
}

static VmaAllocationCreateFlags get_allocation_flags(BufferType buf_type) {
    switch (buf_type) {
    case BufferType::MappedUniformBuffer: return VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
    case BufferType::StorageBufferDynamic: return VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
    default:
        return {};
    }
}

static const char* buf_type_string(BufferType buf_type) {
    switch (buf_type) {
    case BufferType::IndexBuffer: return "IndexBuffer";
    case BufferType::IndexBufferDynamic: return "IndexBufferDynamic";
    case BufferType::MappedUniformBuffer: return "MappedUniformBuffer";
    case BufferType::StorageBufferDynamic: return "StorageBufferDynamic";
    case BufferType::StorageBufferStatic: return "StorageBufferStatic";
    case BufferType::TransferBuffer: return "TransferBuffer";
    case BufferType::VertexBuffer: return "VertexBuffer";
    case BufferType::VertexBufferDynamic: return "VertexBufferDynamic";
    case BufferType::Undefined: return "Undefined";
    default: return "Unknown";
    }
}

// A buffer type has a persistent mapping if it was created with the VMA_ALLOCATION_CREATE_MAPPED_BIT flag set
static bool has_persistent_mapping(BufferType type) {
    return get_allocation_flags(type) & VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
}

static void log_buffer_create(VulkanContext& ctx, RawBuffer const& buffer) {
    ctx.logger->write_fmt(log::Severity::Info, "Created buffer {} with type {} and size {} bytes.", 
        reinterpret_cast<void*>(memory_util::to_vk_type(buffer.buffer)), buf_type_string(buffer.type), buffer.size);
}

static void log_buffer_destroy(VulkanContext& ctx, RawBuffer const& buffer) {
    ctx.logger->write_fmt(log::Severity::Info, "Destroying buffer {} with type {} and size {} bytes.",
        reinterpret_cast<void*>(memory_util::to_vk_type(buffer.buffer)), buf_type_string(buffer.type), buffer.size);
}

static void log_buffer_copy(VulkanContext& ctx, RawBuffer const& src, RawBuffer const& dst, vk::DeviceSize size) {
    ctx.logger->write_fmt(log::Severity::Info, "Issued buffer copy command with size {} bytes, source buffer {} and destination buffer {}.",
        size, reinterpret_cast<void*>(memory_util::to_vk_type(src.buffer)), reinterpret_cast<void*>(memory_util::to_vk_type(dst.buffer)));
}

RawBuffer create_buffer(VulkanContext& ctx, vk::DeviceSize size, BufferType buf_type) {
    if (buf_type == BufferType::Undefined) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to create buffer with buffer type Undefined");
        return {};
    }

    if (size == 0) {
        ctx.logger->write_fmt(log::Severity::Warning, "Tried to create buffer with size 0 bytes");
        return {};
    }

    RawBuffer buffer;
    buffer.size = size;
    buffer.type = buf_type;

    vk::BufferCreateInfo info;
    info.size = size;
    info.usage = get_usage_flags(buf_type);
    info.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = get_memory_usage(buf_type);
    alloc_info.flags = get_allocation_flags(buf_type) | VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    vmaCreateBuffer(ctx.allocator, reinterpret_cast<VkBufferCreateInfo const*>(&info), &alloc_info, 
            reinterpret_cast<VkBuffer*>(&buffer.buffer), &buffer.memory, nullptr);

    log_buffer_create(ctx, buffer);

    return buffer;
}

BufferSlice whole_buffer_slice(RawBuffer& buffer) {
    return BufferSlice{ buffer.buffer, 0, buffer.size };
}

void destroy_buffer(VulkanContext& ctx, RawBuffer& buffer) {
    if (!is_valid_buffer(buffer)) {
        ctx.logger->write_fmt(log::Severity::Warning, "Tried to free invalid buffer.");
        return;
    }

    log_buffer_destroy(ctx, buffer);
    vmaDestroyBuffer(ctx.allocator, buffer.buffer, buffer.memory);
    buffer.size = 0;
    buffer.buffer = nullptr;
    buffer.memory = nullptr;
}

bool is_valid_buffer(RawBuffer const& buffer) {
    return buffer.type != BufferType::Undefined && buffer.size > 0 && buffer.buffer && buffer.memory;
}

std::byte* map_memory(VulkanContext& ctx, RawBuffer& buffer) {
    if (!is_valid_buffer(buffer)) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to obtain memory map of invalid buffer");
        return nullptr;
    }

    if (has_persistent_mapping(buffer.type)) {
        VmaAllocationInfo alloc_info;
        vmaGetAllocationInfo(ctx.allocator, buffer.memory, &alloc_info);
        return static_cast<std::byte*>(alloc_info.pMappedData);
    }

    void* ptr;
    vmaMapMemory(ctx.allocator, buffer.memory, &ptr);
    return static_cast<std::byte*>(ptr);
}

void flush_memory(VulkanContext& ctx, RawBuffer& buffer, vk::DeviceSize offset, vk::DeviceSize size) {
    if (!is_valid_buffer(buffer)) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to flush memory of invalid buffer");
        return;
    }

    if (size != VK_WHOLE_SIZE && offset + size > buffer.size) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to flush memory range with offset {} bytes and size {} bytes, "
            "but buffer is only {} bytes large", offset, size, buffer.size);
        return;
    }

    vmaFlushAllocation(ctx.allocator, buffer.memory, offset, size);
}

void unmap_memory(VulkanContext& ctx, RawBuffer& buffer) {
    if (!is_valid_buffer(buffer)) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to unmap invalid buffer");
        return;
    }

    if (has_persistent_mapping(buffer.type)) {
        ctx.logger->write_fmt(log::Severity::Warning, "Unmapping a persistently mapped buffer");
    }

    vmaUnmapMemory(ctx.allocator, buffer.memory);
}

bool ensure_buffer_size(VulkanContext& ctx, RawBuffer& buffer, vk::DeviceSize requested_size) {
    if (!is_valid_buffer(buffer)) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to resize an invalid buffer");
    }

    if (buffer.size >= requested_size) { return false; }

    BufferType buf_type = buffer.type;
    destroy_buffer(ctx, buffer);
    buffer = create_buffer(ctx, requested_size, buf_type);

    return true;
}

void copy_buffer(VulkanContext& ctx, RawBuffer const& src, RawBuffer& dst, vk::DeviceSize size) {
    log_buffer_copy(ctx, src, dst, size);

    if (!is_valid_buffer(src)) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to do a buffer copy, but source buffer is invalid");
    }

    if (!is_valid_buffer(dst)) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to do a buffer copy, but destination buffer is invalid");
    }

    if (size == 0) {
        ctx.logger->write_fmt(log::Severity::Error, "Tried to do a buffer copy, but size is 0");
    }

    if (size > src.size) {
        ctx.logger->write_fmt(log::Severity::Error,
            "Tried to do a buffer copy, but size ({} bytes) is larger than source buffer size ({} bytes)", size, src.size);
    }

    if (size > dst.size) {
        ctx.logger->write_fmt(log::Severity::Error,
            "Tried to do a buffer copy, but size ({} bytes) is larger than destination buffer size ({} bytes)", size, dst.size);
    }

    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandPool = ctx.command_pool;
    alloc_info.commandBufferCount = 1;

    vk::CommandBuffer cmd_buffer = ctx.device.allocateCommandBuffers(alloc_info)[0];

    // Record copy command buffer
    vk::CommandBufferBeginInfo begin_info;
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd_buffer.begin(begin_info);
    vk::BufferCopy copy;
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = size;
    cmd_buffer.copyBuffer(src.buffer, dst.buffer, copy);
    cmd_buffer.end();
   
    // Submit it
    vk::SubmitInfo submit;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_buffer;
    ctx.graphics_queue.submit(submit, nullptr);
    ctx.device.waitIdle();

    ctx.device.freeCommandBuffers(ctx.command_pool, cmd_buffer);
}

}
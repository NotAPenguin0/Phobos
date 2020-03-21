#ifndef PHOBOS_INSTANCING_BUFFER_HPP_
#define PHOBOS_INSTANCING_BUFFER_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/core/vulkan_context.hpp>

namespace ph {

// Buffer class capable of storing instance data on the GPU in a shader storage buffer. Reallocates memory when nessecary.
// TODO: If a max_size is specified, the buffer can assume that this size will never be exceeded. 
class InstancingBuffer {
public:
    InstancingBuffer() = default;
    InstancingBuffer(VulkanContext& ctx);

    InstancingBuffer(InstancingBuffer&&) = default;
    InstancingBuffer& operator=(InstancingBuffer&&) = default;

    // TODO: Implement max_size
    void create(size_t initial_size, size_t max_size = 0);
    void destroy();

    vk::Buffer buffer_handle();
    vk::DeviceMemory memory_handle();

    size_t size() const;

    void write_data(vk::DescriptorSet descriptor_set, void const* data, size_t size, size_t offset = 0);

private:
    VulkanContext* ctx;

    vk::Buffer buffer = nullptr;
    vk::DeviceMemory memory = nullptr;

    vk::DeviceSize current_size = 0;
    void* data_ptr = nullptr;

};

}

#endif
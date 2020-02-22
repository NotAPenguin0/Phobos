#ifndef PHOBOS_MESH_HPP_
#define PHOBOS_MESH_HPP_

#include <vulkan/vulkan.hpp>
#include <phobos/core/vulkan_context.hpp>

namespace ph {

class Mesh {
public:
    Mesh(VulkanContext& ctx);

    struct CreateInfo {
        VulkanContext* ctx;
        float* vertices;
        size_t vertex_count;
        uint32_t* indices;
        size_t index_count;
        size_t vertex_size;
    };

    Mesh(CreateInfo const& info);
    Mesh(Mesh&& rhs);

    Mesh& operator=(Mesh&& rhs);

    ~Mesh();

    void create(CreateInfo const& info);
    void destroy();

    vk::Buffer get_vertices() const;
    vk::Buffer get_indices() const;

    size_t get_vertex_count() const;
    size_t get_index_count() const;

    vk::DeviceMemory get_memory_handle() const;
    vk::DeviceMemory get_index_memory() const;

private:
    VulkanContext* ctx = nullptr;

    size_t vertex_count = 0;
    size_t index_count;
    vk::Buffer buffer = nullptr;
    vk::DeviceMemory memory = nullptr;

    vk::Buffer index_buffer = nullptr;
    vk::DeviceMemory index_buffer_memory = nullptr;

    void create_vertex_buffer(CreateInfo const& info);
    void create_index_buffer(CreateInfo const& info);
};

}

#endif
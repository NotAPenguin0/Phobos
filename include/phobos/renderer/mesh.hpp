#ifndef PHOBOS_MESH_HPP_
#define PHOBOS_MESH_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/util/buffer_util.hpp>

namespace ph {

class Mesh {
public:
    Mesh() = default;
    Mesh(VulkanContext& ctx);

    struct CreateInfo {
        VulkanContext* ctx;
        float const* vertices;
        size_t vertex_count;
        uint32_t const* indices;
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

    VmaAllocation get_memory_handle() const;
    VmaAllocation get_index_memory() const;

private:
    VulkanContext* ctx = nullptr;

    size_t vertex_count = 0;
    size_t index_count;

    RawBuffer vertex_buffer;
    RawBuffer index_buffer;

    void create_vertex_buffer(CreateInfo const& info);
    void create_index_buffer(CreateInfo const& info);
};

}

#endif
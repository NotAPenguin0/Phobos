#ifndef PHOBOS_RENDER_GRAPH_HPP_
#define PHOBOS_RENDER_GRAPH_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/renderer/mesh.hpp>

namespace ph {

struct RenderGraph {
    vk::ClearColorValue clear_color = vk::ClearColorValue{std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}}};

    // TODO: Proper resource referencing instead of raw pointers?
    std::vector<Mesh*> meshes;

    struct DrawCommand {
        size_t mesh_index;
    };

    std::vector<DrawCommand> draw_commands;
};

}

#endif
#ifndef PHOBOS_RENDER_GRAPH_HPP_
#define PHOBOS_RENDER_GRAPH_HPP_

#include <vulkan/vulkan.hpp>
#include <glm/mat4x4.hpp>

#include <phobos/renderer/mesh.hpp>
#include <phobos/renderer/material.hpp>

#include <phobos/assets/asset_manager.hpp>

namespace ph {

struct RenderGraph {
    vk::ClearColorValue clear_color = vk::ClearColorValue{std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}}};

    // Camera data
    glm::mat4 projection;
    glm::mat4 view;

    AssetManager* asset_manager;

    std::vector<Material*> materials;

    struct Instance {
        glm::mat4 transform;
    };

    struct DrawCommand {
        Handle<Mesh> mesh;
        uint32_t material_index;
        std::vector<Instance> instances;
    };

    std::vector<DrawCommand> draw_commands;
};

}

#endif
#ifndef PHOBOS_RENDER_GRAPH_HPP_
#define PHOBOS_RENDER_GRAPH_HPP_

#include <vulkan/vulkan.hpp>
#include <glm/mat4x4.hpp>

#include <phobos/renderer/mesh.hpp>
#include <phobos/renderer/material.hpp>
#include <phobos/renderer/light.hpp>

#include <stl/vector.hpp>

namespace ph {

struct RenderGraph {
    vk::ClearColorValue clear_color = vk::ClearColorValue{std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}}};

    // Camera data
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 camera_pos;

    stl::vector<Material> materials;
    stl::vector<PointLight> point_lights; 
    // Must have the same size as draw_commands
    stl::vector<glm::mat4> transforms;

    struct DrawCommand {
        Mesh* mesh;
        uint32_t material_index;
    };

    stl::vector<DrawCommand> draw_commands;
};

}

#endif
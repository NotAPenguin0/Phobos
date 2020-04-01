#ifndef PHOBOS_RENDER_PASS_HPP_
#define PHOBOS_RENDER_PASS_HPP_

#include <stl/vector.hpp>
#include <functional>

#include <phobos/renderer/render_attachment.hpp>
#include <phobos/renderer/render_target.hpp>

#include <phobos/renderer/mesh.hpp>
#include <phobos/renderer/material.hpp>
#include <phobos/renderer/light.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace ph {

struct RenderPass {
    // TODO: Multiple pipelines
    std::string pipeline_name;

    stl::vector<RenderAttachment> sampled_attachments;
    stl::vector<RenderAttachment> outputs;

    vk::ClearColorValue clear_color = vk::ClearColorValue{std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}}};

    // Camera data

    // Must have the same size as draw_commands
    stl::vector<glm::mat4> transforms;

    struct DrawCommand {
        Mesh* mesh;
        uint32_t material_index;
    };

    stl::vector<DrawCommand> draw_commands;

    // This callback is called right before executing the renderpass' draw commands
    std::function<void(RenderPass&, vk::CommandBuffer&)> callback = [](RenderPass&, vk::CommandBuffer&) {};

    // These are all modified by the render graph. Don't manually set these fields.
    vk::RenderPass render_pass;
    RenderTarget target;
    stl::uint32_t transforms_offset = 0;
    vk::Pipeline pipeline;
    vk::PipelineLayout pipeline_layout;
};

}

#endif
#ifndef PHOBOS_RENDER_PASS_HPP_
#define PHOBOS_RENDER_PASS_HPP_

#include <stl/vector.hpp>
#include <functional>

#include <phobos/renderer/render_attachment.hpp>
#include <phobos/renderer/render_target.hpp>
#include <phobos/renderer/command_buffer.hpp>

#include <phobos/renderer/mesh.hpp>
#include <phobos/renderer/material.hpp>
#include <phobos/renderer/light.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <phobos/pipeline/pipeline.hpp>

namespace ph {

struct RenderPass {
    // This is the name that will be given to the vk object created from this renderpass
    std::string debug_name;

    std::vector<RenderAttachment> sampled_attachments;
    std::vector<RenderAttachment> outputs;
    // Size of this must match the size of the outputs vector
    std::vector<vk::ClearValue> clear_values;

    struct DrawCommand {
        Mesh* mesh = nullptr;
        uint32_t material_index = 0;
    };
    stl::vector<DrawCommand> draw_commands;
    // Must have the same size as draw_commands
    stl::vector<glm::mat4> transforms;


    // This callback is called right when executing the renderpass
    std::function<void(CommandBuffer&)> callback = [](CommandBuffer&) {};

private:
    // These classes and functions all need access to the render_pass field
    friend class CommandBuffer;
    friend class RenderGraph;
    friend class Renderer;
    friend Pipeline create_or_get_pipeline(VulkanContext* ctx, RenderPass* pass, PipelineCreateInfo pci);

    vk::RenderPass render_pass;
    RenderTarget target;
    stl::uint32_t transforms_offset = 0;

    Pipeline active_pipeline;
    bool active = false;
};

}

#endif
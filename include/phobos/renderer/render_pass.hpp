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
#include <phobos/renderer/cubemap.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <phobos/pipeline/pipeline.hpp>

namespace ph {

struct RenderPass {
    // This is the name that will be given to the vk object created from this renderpass
    std::string debug_name;

    std::vector<RenderAttachment> sampled_attachments;
    std::vector<RenderAttachment> outputs;
    // Size of this must match the size of the amount of attachments that have to be cleared
    std::vector<vk::ClearValue> clear_values;

    // This callback is called right when executing the renderpass
    std::function<void(CommandBuffer&)> callback = [](CommandBuffer&) {};
    std::function<void(CommandBuffer&)> pre_callback = [](CommandBuffer&) {};

    RenderTarget const& get_target() const { return target; }
    bool is_active() const { return active; }

    vk::RenderPass render_pass;
    RenderTarget target;

    bool active = false;

private:
    friend Pipeline create_or_get_pipeline(VulkanContext* ctx, RenderPass* pass, PipelineCreateInfo pci);

};

}

#endif
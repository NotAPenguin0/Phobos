#ifndef PHOBOS_RENDERER_HPP_
#define PHOBOS_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>

#include <phobos/renderer/render_graph.hpp>
#include <phobos/renderer/command_buffer.hpp>

#include <phobos/events/event_listener.hpp>

// TODO: Custom command buffer class?

namespace ph {

class Renderer : public EventListener<InstancingBufferResizeEvent> {
public:
    Renderer(VulkanContext& context);

    void render_frame(FrameInfo& info);

    // This can only be called inside the renderpass callback.
    // The descriptor set layout will be set by this function. The pNext parameter will be passed into the vk::DescriptorSetAllocateInfo::pNext
    // field.
    vk::DescriptorSet get_descriptor(FrameInfo& frame, RenderPass& pass, DescriptorSetBinding set_binding, void* pNext = nullptr);

    // Execute the draw commands in the pass::draw_commands array with the default fixed rendering pipeline.
    // Typically you want to call this function when you simply want to render the submitted draws without any special processing.
    void execute_draw_commands(FrameInfo& frame, CommandBuffer& cmd_buffer);

    Pipeline get_pipeline(std::string_view name, RenderPass& pass);

    void destroy();

protected:
    void on_event(InstancingBufferResizeEvent const& e) override;
private:
    VulkanContext& ctx;

    vk::DescriptorSet get_descriptor(FrameInfo& frame, DescriptorSetLayoutCreateInfo const& set_layout, 
        DescriptorSetBinding set_binding, void* pNext = nullptr);

    void update_camera_data(FrameInfo& info, RenderGraph const* graph);
    void update_model_matrices(FrameInfo& info, RenderGraph const* graph);
    void update_lights(FrameInfo& info, RenderGraph const* graph);

    vk::DescriptorSet get_fixed_descriptor_set(FrameInfo& frame, RenderGraph const* graph);
};

}

#endif
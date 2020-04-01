#ifndef PHOBOS_RENDERER_HPP_
#define PHOBOS_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>

#include <phobos/renderer/render_graph.hpp>

#include <phobos/events/event_listener.hpp>

namespace ph {

class Renderer : public EventListener<InstancingBufferResizeEvent> {
public:
    Renderer(VulkanContext& context);

    void render_frame(FrameInfo& info);
    // Execute the draw commands in the pass::draw_commands array with the default fixed rendering pipeline.
    // Typically you want to call this function when you simply want to render the submitted draws without any special processing
    void execute_draw_commands(FrameInfo& frame, RenderPass& pass, vk::CommandBuffer& cmd_buffer);

    void destroy();

protected:
    void on_event(InstancingBufferResizeEvent const& e) override;
private:
    VulkanContext& ctx;

    void update_camera_data(FrameInfo& info, RenderGraph const* graph);
    void update_model_matrices(FrameInfo& info, RenderGraph const* graph);
    void update_materials(FrameInfo& info, RenderGraph const* graph);
    void update_lights(FrameInfo& info, RenderGraph const* graph);
};

}

#endif
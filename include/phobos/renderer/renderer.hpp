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

    void render_frame(FrameInfo& info, RenderGraph const& graph);

    void destroy();

protected:
    void on_event(InstancingBufferResizeEvent const& e) override;
private:
    VulkanContext& ctx;

    void update_pv_matrix(FrameInfo& info, RenderGraph const& graph);
    void update_model_matrices(FrameInfo& info, RenderGraph::DrawCommand const& model);
    void update_materials(FrameInfo& info, RenderGraph const& graph);
};

}

#endif
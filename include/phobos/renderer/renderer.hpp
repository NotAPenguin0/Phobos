#ifndef PHOBOS_RENDERER_HPP_
#define PHOBOS_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>

#include <phobos/renderer/render_graph.hpp>

namespace ph {

class Renderer {
public:
    Renderer(VulkanContext& context);

    void render_frame(FrameInfo& info, RenderGraph const& graph);

    void destroy();

private:
    VulkanContext& ctx;

    void update_pv_matrix(FrameInfo& info, RenderGraph const& graph);
    void update_model_matrices(FrameInfo& info, RenderGraph::DrawCommand const& model);
};

}

#endif
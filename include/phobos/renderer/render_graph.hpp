#ifndef PHOBOS_RENDER_GRAPH_HPP_
#define PHOBOS_RENDER_GRAPH_HPP_

#include <phobos/renderer/render_pass.hpp>
#include <phobos/present/frame_info.hpp>
#include <stl/vector.hpp>

namespace ph {

class RenderGraph {
public:
    friend class Renderer;
    RenderGraph(VulkanContext* ctx);

    void add_pass(RenderPass&& pass);
    void build();

private:
    VulkanContext* ctx;
    stl::vector<RenderPass> passes;

    void create_render_passes();
};

}

#endif
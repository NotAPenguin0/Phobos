#ifndef PHOBOS_RENDER_GRAPH_HPP_
#define PHOBOS_RENDER_GRAPH_HPP_

#include <phobos/renderer/render_pass.hpp>
#include <phobos/present/frame_info.hpp>
#include <vector>

namespace ph {

class RenderGraph {
public:
    RenderGraph(VulkanContext* ctx, PerThreadContext* ptc);

    void add_pass(RenderPass&& pass);
    void build();

    std::vector<RenderPass> passes;
private:
    VulkanContext* ctx;
    PerThreadContext* ptc;
    void create_render_passes();
};

}

#endif
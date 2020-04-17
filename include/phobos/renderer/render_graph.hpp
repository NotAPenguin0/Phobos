#ifndef PHOBOS_RENDER_GRAPH_HPP_
#define PHOBOS_RENDER_GRAPH_HPP_

#include <phobos/renderer/render_pass.hpp>
#include <phobos/present/frame_info.hpp>
#include <stl/vector.hpp>

namespace ph {

class RenderGraph {
public:
    friend class Renderer;

    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 camera_pos;

    stl::vector<Material> materials;
    stl::vector<PointLight> point_lights; 
    stl::vector<DirectionalLight> directional_lights;

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
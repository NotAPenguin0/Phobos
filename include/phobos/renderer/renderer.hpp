#ifndef PHOBOS_RENDERER_HPP_
#define PHOBOS_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>

namespace ph {

class Renderer {
public:
    Renderer(VulkanContext& context);

    void render_frame(FrameInfo& info);

private:
    VulkanContext& ctx;
};

}

#endif
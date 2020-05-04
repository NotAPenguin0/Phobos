#ifndef PHOBOS_RENDERER_HPP_
#define PHOBOS_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>

#include <phobos/renderer/render_graph.hpp>
#include <phobos/renderer/command_buffer.hpp>
#include <phobos/renderer/meta.hpp>

#include <phobos/events/event_listener.hpp>


namespace ph {

struct BuiltinUniforms {
    BufferSlice camera;
    BufferSlice lights;
};

struct DefaultTextures {
    Texture color;
    Texture normal;
    Texture specular;
};

class Renderer {
public:
    Renderer(VulkanContext& context);

    void render_frame(FrameInfo& info);
    DefaultTextures& get_default_textures();

    void destroy();
private:
    VulkanContext& ctx;
    DefaultTextures default_textures;
};

}

#endif
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

    // This can only be called inside the renderpass callback.
    // The descriptor set layout will be set by this function. The pNext parameter will be passed into the vk::DescriptorSetAllocateInfo::pNext
    // field.
    vk::DescriptorSet get_descriptor(FrameInfo& frame, RenderPass& pass, DescriptorSetBinding set_binding, void* pNext = nullptr);

    // Get a named pipeline. Must be called in an active renderpass
    Pipeline get_pipeline(std::string_view name, RenderPass& pass);

    BuiltinUniforms get_builtin_uniforms();
    DefaultTextures& get_default_textures();

    void destroy();
private:
    VulkanContext& ctx;
    BuiltinUniforms builtin_uniforms;
    DefaultTextures default_textures;

    vk::DescriptorSet get_descriptor(FrameInfo& frame, DescriptorSetLayoutCreateInfo const& set_layout, 
        DescriptorSetBinding set_binding, void* pNext = nullptr);

    void update_camera_data(CommandBuffer& cmd_buf, RenderGraph const* graph);
    void update_lights(CommandBuffer& cmd_buf, RenderGraph const* graph);

};

}

#endif
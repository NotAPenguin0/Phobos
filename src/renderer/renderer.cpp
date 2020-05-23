#include <phobos/renderer/renderer.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <phobos/renderer/meta.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/present/present_manager.hpp>

#include <stl/enumerate.hpp>


namespace ph {

static Texture create_single_color_texture(VulkanContext& ctx, uint8_t r, uint8_t g, uint8_t b, vk::Format fmt) {
    Texture::CreateInfo info;
    info.channels = 4;
    info.ctx = &ctx;
    const uint8_t data[] = { r, g, b, 255 };
    info.data = data;
    info.width = 1;
    info.height = 1;
    info.format = fmt;
    return Texture(info);
}

Renderer::Renderer(VulkanContext& context) : ctx(context) {
    default_textures.color = create_single_color_texture(ctx, 255, 0, 255, vk::Format::eR8G8B8A8Srgb);
    default_textures.specular = create_single_color_texture(ctx, 0, 0, 0, vk::Format::eR8G8B8A8Srgb);
    default_textures.normal = create_single_color_texture(ctx, 0, 255, 0, vk::Format::eR8G8B8A8Unorm);
} 


void Renderer::render_frame(FrameInfo& info) {
    // Grab thread context of the main thread
    CommandBuffer cmd_buffer { &ctx, &info, &ctx.thread_contexts[0], info.command_buffer };
    cmd_buffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    for (auto& pass : info.render_graph->passes) {
        cmd_buffer.begin_renderpass(pass);
        pass.callback(cmd_buffer);
        cmd_buffer.end_renderpass();
    }

    cmd_buffer.end();
}

DefaultTextures& Renderer::get_default_textures() {
    return default_textures;
}

void Renderer::destroy() {
    default_textures.color.destroy();
    default_textures.specular.destroy();
    default_textures.normal.destroy();
}


} // namespace ph

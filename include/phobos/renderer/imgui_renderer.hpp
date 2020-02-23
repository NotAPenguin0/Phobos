#ifndef PHOBOS_IMGUI_RENDERER_HPP_
#define PHOBOS_IMGUI_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>

namespace ph {

class ImGuiRenderer : public EventListener<SwapchainRecreateEvent> {
public:
    ImGuiRenderer(WindowContext& window_ctx, VulkanContext& context);

    void destroy();
    // Must be called at the start of each frame
    void begin_frame();
    // Records command buffers and writes them to the FrameInfo struct
    void render_frame(FrameInfo& info);
protected:
    void on_event(SwapchainRecreateEvent const& evt) override;
private:
    VulkanContext& ctx;
    vk::DescriptorPool descriptor_pool;
    vk::CommandPool command_pool;

    std::vector<vk::CommandBuffer> cmd_buffers;
    vk::RenderPass render_pass;

    std::vector<vk::Framebuffer> framebuffers;
};

}

#endif
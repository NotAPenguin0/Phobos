#ifndef PHOBOS_RENDER_TARGET_HPP_
#define PHOBOS_RENDER_TARGET_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/renderer/render_attachment.hpp>

#include <vector>

namespace ph {

class RenderTarget {
public:
    RenderTarget() = default;
    RenderTarget(VulkanContext* ctx);
    RenderTarget(VulkanContext* ctx, vk::RenderPass render_pass, std::vector<RenderAttachment> const& attachments);

    RenderTarget(RenderTarget&& rhs);
    RenderTarget& operator=(RenderTarget&& rhs);

    ~RenderTarget();

    void destroy();

    vk::Framebuffer get_framebuf() const {
        return framebuffer;
    }

private:
    VulkanContext* ctx;
    vk::Framebuffer framebuffer = nullptr;
};

} // namespace ph 

#endif
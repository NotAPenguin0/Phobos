#ifndef PHOBOS_RENDER_TARGET_HPP_
#define PHOBOS_RENDER_TARGET_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/renderer/render_attachment.hpp>

#include <stl/vector.hpp>

namespace ph {

class RenderTarget {
public:
    RenderTarget() = default;
    RenderTarget(VulkanContext* ctx);
    RenderTarget(VulkanContext* ctx, vk::RenderPass render_pass, stl::vector<RenderAttachment> const& attachments);

    RenderTarget(RenderTarget&& rhs);
    RenderTarget& operator=(RenderTarget&& rhs);

    ~RenderTarget();

    void destroy();

    vk::Framebuffer get_framebuf() const {
        return framebuffer;
    }

    std::uint32_t get_width() const {
        return width;
    }

    std::uint32_t get_height() const {
        return height;
    }

private:
    VulkanContext* ctx;
    vk::Framebuffer framebuffer = nullptr;
    std::uint32_t width = 0, height = 0;
};

} // namespace ph 

#endif
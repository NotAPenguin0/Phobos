#include <phobos/renderer/render_target.hpp>

#undef max
#undef min


namespace ph {

RenderTarget::RenderTarget(VulkanContext* ctx) : ctx(ctx) {

}

RenderTarget::RenderTarget(VulkanContext* ctx, vk::RenderPass render_pass, std::vector<RenderAttachment> const& attachments) :
    ctx(ctx) {

    vk::FramebufferCreateInfo info;
    std::vector<vk::ImageView> views;
    views.reserve(attachments.size());
    for (auto const& attachment : attachments) {
        views.push_back(attachment.image_view());
        info.width = std::max(attachment.get_width(), info.width);
        info.height = std::max(attachment.get_height(), info.height);
    }
    info.renderPass = render_pass;
    info.attachmentCount = views.size();
    info.pAttachments = views.data();
    info.layers = 1;
    framebuffer = ctx->device.createFramebuffer(info);
}

RenderTarget::RenderTarget(RenderTarget&& rhs) {
    destroy();
    ctx = rhs.ctx;
    framebuffer = rhs.framebuffer;
    rhs.framebuffer = nullptr;
}

RenderTarget& RenderTarget::operator=(RenderTarget&& rhs) {
    if (this != &rhs) {
        destroy();
        ctx = rhs.ctx;
        framebuffer = rhs.framebuffer;
        rhs.framebuffer = nullptr;
    }
    return *this;
}

RenderTarget::~RenderTarget() {
    destroy();
}

void RenderTarget::destroy() {
    if (framebuffer) {
        ctx->device.destroyFramebuffer(framebuffer);
        framebuffer = nullptr;
    }
}

}
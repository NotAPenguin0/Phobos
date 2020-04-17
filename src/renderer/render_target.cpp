#include <phobos/renderer/render_target.hpp>

#include <stl/vector.hpp>

#undef max
#undef min


namespace ph {

RenderTarget::RenderTarget(VulkanContext* ctx) : ctx(ctx) {

}

RenderTarget::RenderTarget(VulkanContext* ctx, vk::RenderPass render_pass, std::vector<RenderAttachment> const& attachments) :
    ctx(ctx) {

    vk::FramebufferCreateInfo info;
    stl::vector<vk::ImageView> views;
    views.reserve(attachments.size());
    for (auto const& attachment : attachments) {
        views.push_back(attachment.image_view().view);
        info.width = std::max(attachment.get_width(), info.width);
        info.height = std::max(attachment.get_height(), info.height);
    }
    info.renderPass = render_pass;
    info.attachmentCount = views.size();
    info.pAttachments = views.data();
    info.layers = 1;

    auto framebuf = ctx->framebuffer_cache.get(info);
    if (framebuf) {
        framebuffer = *framebuf;
    } else {
        framebuffer = ctx->device.createFramebuffer(info);
        ctx->framebuffer_cache.insert(info, stl::move(framebuffer));
    }
    
    width = info.width;
    height = info.height;
}

RenderTarget::RenderTarget(RenderTarget&& rhs) {
    destroy();
    ctx = rhs.ctx;
    framebuffer = rhs.framebuffer;
    width = rhs.width;
    height = rhs.height;
    rhs.framebuffer = nullptr;
    rhs.width = 0;
    rhs.height = 0;
}

RenderTarget& RenderTarget::operator=(RenderTarget&& rhs) {
    if (this != &rhs) {
        destroy();
        ctx = rhs.ctx;
        framebuffer = rhs.framebuffer;
        width = rhs.width;
        height = rhs.height;
        rhs.framebuffer = nullptr;
        rhs.width = 0;
        rhs.height = 0;
    }
    return *this;
}

RenderTarget::~RenderTarget() {
    destroy();
}

void RenderTarget::destroy() {
    // No longer destroy, as created framebuffers are now owned by the cache
}

}
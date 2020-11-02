#include <phobos/renderer/render_attachment.hpp>

#include <phobos/util/image_util.hpp>

namespace ph {

RenderAttachment::RenderAttachment(VulkanContext* ctx) : ctx(ctx) {
    ctx->event_dispatcher.add_listener(this);
}

RenderAttachment::RenderAttachment(VulkanContext* ctx, RawImage& image, ImageView view, vk::ImageAspectFlags aspect)
        : ctx(ctx), owning(true), image(image), view(view), aspect(aspect) {
    ctx->event_dispatcher.add_listener(this);
}

RenderAttachment::RenderAttachment(RenderAttachment const& rhs) :
    ctx(rhs.ctx), owning(false), image(rhs.image), aspect(rhs.aspect), view(rhs.view) {
    ctx->event_dispatcher.add_listener(this);
}

RenderAttachment::RenderAttachment(RenderAttachment&& rhs) : 
    ctx(rhs.ctx), owning(rhs.owning), image(rhs.image), view(rhs.view), aspect(rhs.aspect) {
    ctx->event_dispatcher.add_listener(this);
    ctx->event_dispatcher.remove_listener(&rhs);
    rhs.owning = false;
    rhs.view = {};
    rhs.image = {};
    rhs.aspect = {};
}

RenderAttachment& RenderAttachment::operator=(RenderAttachment const& rhs) {
    if (this != &rhs) {
        destroy();
        ctx = rhs.ctx;
        ctx->event_dispatcher.add_listener(this);
        owning = false;
        image = rhs.image;    
        view = rhs.view;
        aspect = rhs.aspect;
    }
    return *this;
}

RenderAttachment& RenderAttachment::operator=(RenderAttachment&& rhs) {
    if (this != &rhs) {
        destroy();
        ctx = rhs.ctx;
        owning = rhs.owning;
        image = rhs.image;
        view = rhs.view;
        aspect = rhs.aspect;

        ctx->event_dispatcher.add_listener(this);

        rhs.owning = false;
        rhs.image = {};
        rhs.view = {};
        rhs.aspect = {};
        ctx->event_dispatcher.remove_listener(&rhs);
    }
    return *this;
}

RenderAttachment::~RenderAttachment() {
    destroy();
}

RenderAttachment RenderAttachment::from_ref(VulkanContext* ctx, RawImage& image, ImageView view) {
    RenderAttachment attachment(ctx);
    // NOTE: RenderAttachment constructor already adds the attachment to the listener list!
    attachment.owning = false;
    attachment.image = image;
    attachment.view = view;

    return attachment;
}

void RenderAttachment::on_event(SwapchainRecreateEvent const& evt) {
    // If this attachment refers to a swapchain image, update the size members
    if (std::find(ctx->swapchain.image_views.begin(), ctx->swapchain.image_views.end(), view) !=
        ctx->swapchain.image_views.end()) {
            
        image.size.width = ctx->swapchain.extent.width;
        image.size.height = ctx->swapchain.extent.height;
    }
}

void RenderAttachment::destroy() {
    if (!ctx) return;

    ctx->event_dispatcher.remove_listener(this);
    if (owning) {
        destroy_image_view(*ctx, view);
        destroy_image(*ctx, image);
        owning = false;
    }

    ctx = nullptr;
}

void RenderAttachment::resize(uint32_t new_width, uint32_t new_height) {
    if (!owning) { return; }
    // Don't resize if the attachment already has the correct size
    if (new_width == image.size.width && new_height == image.size.height) { return; }
    // Don't resize if new size is invalid (has a zero)
    if (new_width == 0 || new_height == 0) { return; }

    VulkanContext* ctx_backup = ctx;
    ImageType old_type = image.type;
    vk::Format old_format = image.format;
    vk::SampleCountFlagBits old_samples = image.samples;
    destroy();
    ctx = ctx_backup;

    ctx->event_dispatcher.add_listener(this);

    image = create_image(*ctx, new_width, new_height, old_type, old_format, 1, 1, old_samples);
    view = create_image_view(ctx->device, image, aspect);

    owning = true;
}

}
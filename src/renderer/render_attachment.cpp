#include <phobos/renderer/render_attachment.hpp>

#include <imgui/imgui_impl_phobos.h>
#include <phobos/util/image_util.hpp>

namespace ph {

RenderAttachment::RenderAttachment(VulkanContext* ctx) : ctx(ctx) {
    ctx->event_dispatcher.add_listener(this);
}

RenderAttachment::RenderAttachment(VulkanContext* ctx, vk::Image image, vk::DeviceMemory memory, 
    vk::ImageView view, uint32_t w, uint32_t h, vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect) 
        : ctx(ctx), owning(true), image(image), memory(memory), view(view),
    width(w), height(h), format(format), usage(usage), aspect(aspect) {

    ctx->event_dispatcher.add_listener(this);
    imgui_tex_id = ImGui_ImplPhobos_AddTexture(view, vk::ImageLayout::eShaderReadOnlyOptimal);
}

RenderAttachment::RenderAttachment(RenderAttachment const& rhs) :
    ctx(rhs.ctx), owning(false), image(rhs.image), memory(rhs.memory), view(rhs.view),
    width(rhs.width), height(rhs.height), imgui_tex_id(rhs.imgui_tex_id), format(rhs.format), usage(rhs.usage),
    aspect(rhs.aspect) {
    ctx->event_dispatcher.add_listener(this);
}

RenderAttachment::RenderAttachment(RenderAttachment&& rhs) : 
    ctx(rhs.ctx), owning(rhs.owning), image(rhs.image), memory(rhs.memory), view(rhs.view),
    width(rhs.width), height(rhs.height), imgui_tex_id(rhs.imgui_tex_id), format(rhs.format), usage(rhs.usage),
    aspect(rhs.aspect) {
    ctx->event_dispatcher.add_listener(this);
    ctx->event_dispatcher.remove_listener(&rhs);
    rhs.owning = false;
    rhs.image = nullptr;
    rhs.memory = nullptr;
    rhs.view = nullptr;
    rhs.width = 0;
    rhs.height = 0;
    rhs.imgui_tex_id = nullptr;
    rhs.format = vk::Format::eUndefined;
    rhs.usage = {};
    rhs.aspect = {};
}

RenderAttachment& RenderAttachment::operator=(RenderAttachment const& rhs) {
    if (this != &rhs) {
        destroy();
        ctx = rhs.ctx;
        ctx->event_dispatcher.add_listener(this);
        owning = false;
        image = rhs.image;
        memory = rhs.memory;
        view = rhs.view;
        width = rhs.width;
        height = rhs.height;
        imgui_tex_id = rhs.imgui_tex_id;
        format = rhs.format;
        usage = rhs.usage;
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
        memory = rhs.memory;
        view = rhs.view;
        width = rhs.width;
        height = rhs.height;
        imgui_tex_id = rhs.imgui_tex_id;
        format = rhs.format;
        usage = rhs.usage;
        aspect = rhs.aspect;

        ctx->event_dispatcher.add_listener(this);

        rhs.owning = false;
        rhs.image = nullptr;
        rhs.memory = nullptr;
        rhs.view = nullptr;
        rhs.width = 0;
        rhs.height = 0;
        rhs.imgui_tex_id = nullptr;
        rhs.format = vk::Format::eUndefined;
        rhs.usage = {};
        rhs.aspect = {};
        ctx->event_dispatcher.remove_listener(&rhs);
    }
    return *this;
}

RenderAttachment::~RenderAttachment() {
    destroy();
}

RenderAttachment RenderAttachment::from_ref(VulkanContext* ctx, vk::Image image, vk::DeviceMemory memory, vk::ImageView view,
    uint32_t w, uint32_t h, vk::Format fmt) {

    RenderAttachment attachment(ctx);
    // NOTE: RenderAttachment constructor already adds the attachment to the listener list!
    attachment.owning = false;
    attachment.image = image;
    attachment.memory = memory;
    attachment.view = view;
    attachment.width = w;
    attachment.height = h;
    attachment.format = fmt;

    return attachment;
}

void RenderAttachment::on_event(SwapchainRecreateEvent const& evt) {
    // If this attachment refers to a swapchain image, update the size members
    if (std::find(ctx->swapchain.image_views.begin(), ctx->swapchain.image_views.end(), view) !=
        ctx->swapchain.image_views.end()) {
            
        width = ctx->swapchain.extent.width;
        height = ctx->swapchain.extent.height;
    }
}

void RenderAttachment::destroy() {
    if (!ctx) return;

    ctx->event_dispatcher.remove_listener(this);
    if (owning) {
        ctx->device.destroyImage(image);
        ctx->device.freeMemory(memory);
        ctx->device.destroyImageView(view);
        ImGui_ImplPhobos_RemoveTexture(imgui_tex_id);
        owning = false;
    }

    ctx = nullptr;
}

void RenderAttachment::resize(uint32_t new_width, uint32_t new_height) {
    if (!owning) { return; }
    // Don't resize if the attachment already has the correct size
    if (new_width == width && new_height == height) { return; }

    VulkanContext* ctx_backup = ctx;
    destroy();
    ctx = ctx_backup;

    ctx->event_dispatcher.add_listener(this);

    create_image(*ctx, new_width, new_height, format, vk::ImageTiling::eOptimal, 
        usage, vk::MemoryPropertyFlagBits::eDeviceLocal, image, memory);
    view = create_image_view(ctx->device, image, format, aspect);

    imgui_tex_id = ImGui_ImplPhobos_AddTexture(view, vk::ImageLayout::eShaderReadOnlyOptimal);

    owning = true;

    width = new_width;
    height = new_height;
}

}
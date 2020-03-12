#include <phobos/renderer/render_attachment.hpp>

#include <imgui/imgui_impl_vulkan.h>

namespace ph {

RenderAttachment::RenderAttachment(VulkanContext* ctx) : ctx(ctx) {
    ctx->event_dispatcher.add_listener(this);
}

RenderAttachment::RenderAttachment(VulkanContext* ctx, vk::Image image, vk::DeviceMemory memory, 
    vk::ImageView view, uint32_t w, uint32_t h) : ctx(ctx), owning(true), image(image), memory(memory), view(view),
    width(w), height(h) {

    ctx->event_dispatcher.add_listener(this);
    imgui_tex_id = ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

RenderAttachment::RenderAttachment(RenderAttachment const& rhs) :
    ctx(rhs.ctx), owning(false), image(rhs.image), memory(rhs.memory), view(rhs.view),
    width(rhs.width), height(rhs.height), imgui_tex_id(rhs.imgui_tex_id) {
    ctx->event_dispatcher.add_listener(this);
}

RenderAttachment::RenderAttachment(RenderAttachment&& rhs) : 
    ctx(rhs.ctx), owning(rhs.owning), image(rhs.image), memory(rhs.memory), view(rhs.view),
    width(rhs.width), height(rhs.height), imgui_tex_id(rhs.imgui_tex_id) {
    ctx->event_dispatcher.add_listener(this);
    ctx->event_dispatcher.remove_listener(&rhs);
    rhs.owning = false;
    rhs.image = nullptr;
    rhs.memory = nullptr;
    rhs.view = nullptr;
    rhs.width = 0;
    rhs.height = 0;
    rhs.imgui_tex_id = nullptr;
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

        ctx->event_dispatcher.add_listener(this);

        rhs.owning = false;
        rhs.image = nullptr;
        rhs.memory = nullptr;
        rhs.view = nullptr;
        rhs.width = 0;
        rhs.height = 0;
        rhs.imgui_tex_id = nullptr;
        ctx->event_dispatcher.remove_listener(&rhs);
    }
    return *this;
}

RenderAttachment::~RenderAttachment() {
    destroy();
}

RenderAttachment RenderAttachment::from_ref(VulkanContext* ctx, vk::Image image, vk::DeviceMemory memory, vk::ImageView view,
    uint32_t w, uint32_t h) {

    RenderAttachment attachment(ctx);
    ctx->event_dispatcher.add_listener(&attachment);
    attachment.owning = false;
    attachment.image = image;
    attachment.memory = memory;
    attachment.view = view;
    attachment.width = w;
    attachment.height = h;

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
        ImGui_ImplVulkan_RemoveTexture(imgui_tex_id);
        owning = false;
    }

    ctx = nullptr;
}

}
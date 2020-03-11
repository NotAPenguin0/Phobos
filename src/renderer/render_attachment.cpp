#include <phobos/renderer/render_attachment.hpp>

#include <imgui/imgui_impl_vulkan.h>

namespace ph {

RenderAttachment::RenderAttachment(VulkanContext* ctx) : ctx(ctx) {

}

RenderAttachment::RenderAttachment(VulkanContext* ctx, vk::Image image, vk::DeviceMemory memory, 
    vk::ImageView view, uint32_t w, uint32_t h) : ctx(ctx), owning(true), image(image), memory(memory), view(view),
    width(w), height(h) {

    imgui_tex_id = ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

RenderAttachment::RenderAttachment(RenderAttachment const& rhs) :
    ctx(rhs.ctx), owning(false), image(rhs.image), memory(rhs.memory), view(rhs.view),
    width(rhs.width), height(rhs.height), imgui_tex_id(rhs.imgui_tex_id) {

}

RenderAttachment::RenderAttachment(RenderAttachment&& rhs) : 
    ctx(rhs.ctx), owning(rhs.owning), image(rhs.image), memory(rhs.memory), view(rhs.view),
    width(rhs.width), height(rhs.height), imgui_tex_id(rhs.imgui_tex_id) {
    
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

        rhs.owning = false;
        rhs.image = nullptr;
        rhs.memory = nullptr;
        rhs.view = nullptr;
        rhs.width = 0;
        rhs.height = 0;
        rhs.imgui_tex_id = nullptr;
    }
    return *this;
}

RenderAttachment::~RenderAttachment() {
    destroy();
}

RenderAttachment RenderAttachment::from_ref(VulkanContext* ctx, vk::Image image, vk::DeviceMemory memory, vk::ImageView view,
    uint32_t w, uint32_t h) {
    RenderAttachment attachment(ctx);
    attachment.owning = false;
    attachment.image = image;
    attachment.memory = memory;
    attachment.view = view;
    attachment.width = w;
    attachment.height = h;

    return attachment;
}

void RenderAttachment::destroy() {
    if (owning) {
        ctx->device.destroyImage(image);
        ctx->device.freeMemory(memory);
        ctx->device.destroyImageView(view);
        ImGui_ImplVulkan_RemoveTexture(imgui_tex_id);
    }

    ctx = nullptr;
}

}
#ifndef PHOBOS_RENDER_ATTACHMENT_HPP_
#define PHOBOS_RENDER_ATTACHMENT_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/events/event_dispatcher.hpp>

namespace ph {

class RenderAttachment : public EventListener<SwapchainRecreateEvent> {
public: 
    RenderAttachment() = default;
    RenderAttachment(VulkanContext* ctx);
    RenderAttachment(VulkanContext* ctx, vk::Image image, vk::DeviceMemory memory, vk::ImageView view, uint32_t w, uint32_t h);
    RenderAttachment(RenderAttachment const& rhs);
    RenderAttachment(RenderAttachment&& rhs);

    RenderAttachment& operator=(RenderAttachment const& rhs);
    RenderAttachment& operator=(RenderAttachment&& rhs);

    ~RenderAttachment();

    static RenderAttachment from_ref(VulkanContext* ctx, vk::Image image, vk::DeviceMemory memory, vk::ImageView view, 
        uint32_t w, uint32_t h);

    void destroy();

    vk::Image image_handle() const {
        return image;
    }
    
    vk::DeviceMemory memory_handle() const {
        return memory;
    }

    vk::ImageView image_view() const {
        return view;
    }

    uint32_t get_width() const {
        return width;
    }

    uint32_t get_height() const {
        return height;
    }

    void* get_imgui_tex_id() const {
        return imgui_tex_id;
    }

protected:
    void on_event(SwapchainRecreateEvent const& evt) override;
private:
    VulkanContext* ctx = nullptr;

    // Specifies whether this RenderAttachment owns the image it stores. We need this flag for cleanup
    bool owning = false;

    vk::Image image = nullptr;
    vk::DeviceMemory memory = nullptr;
    vk::ImageView view = nullptr;

    uint32_t width, height;

    void* imgui_tex_id = nullptr;
};

} // namespace ph 

#endif
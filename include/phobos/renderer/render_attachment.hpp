#ifndef PHOBOS_RENDER_ATTACHMENT_HPP_
#define PHOBOS_RENDER_ATTACHMENT_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/events/event_dispatcher.hpp>

#include <phobos/util/image_util.hpp>
#include <phobos/util/memory_util.hpp>

namespace ph {

class RenderAttachment : public EventListener<SwapchainRecreateEvent> {
public: 
    RenderAttachment() = default;
    RenderAttachment(VulkanContext* ctx);
    RenderAttachment(VulkanContext* ctx, RawImage& image, vk::ImageView view, vk::ImageAspectFlags aspect);
    RenderAttachment(RenderAttachment const& rhs);
    RenderAttachment(RenderAttachment&& rhs);

    RenderAttachment& operator=(RenderAttachment const& rhs);
    RenderAttachment& operator=(RenderAttachment&& rhs);

    virtual ~RenderAttachment();

    static RenderAttachment from_ref(VulkanContext* ctx, RawImage& image, vk::ImageView view);

    void destroy();

    void resize(uint32_t new_width, uint32_t new_height);

    vk::Image image_handle() const {
        return image.image;
    }
    
    VmaAllocation memory_handle() const {
        return image.memory;
    }

    vk::ImageView image_view() const {
        return view;
    }

    vk::Extent2D size() const {
        return image.size;
    }

    uint32_t get_width() const {
        return image.size.width;
    }
     
    uint32_t get_height() const {
        return image.size.height;
    }

    void* get_imgui_tex_id() const {
        return reinterpret_cast<void*>(memory_util::to_vk_type(view));
    }

    vk::Format get_format() const {
        return image.format;
    }

protected:
    void on_event(SwapchainRecreateEvent const& evt) override;
private:
    VulkanContext* ctx = nullptr;

    // Specifies whether this RenderAttachment owns the image it stores. We need this flag for cleanup
    bool owning = false;

    RawImage image;
    vk::ImageView view = nullptr;
    vk::ImageAspectFlags aspect = {};
};

} // namespace ph 

#endif
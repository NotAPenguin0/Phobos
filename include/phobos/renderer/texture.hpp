#ifndef PHOBOS_TEXTURE_HPP_
#define PHOBOS_TEXTURE_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/util/image_util.hpp>

namespace ph {

class Texture {
public:
    Texture() = default;
    Texture(VulkanContext& ctx);

    struct CreateInfo {
        VulkanContext* ctx;
        size_t width;
        size_t height;
        uint8_t const* data;
        vk::Format format = vk::Format::eR8G8B8Srgb;
        size_t channels = 4;
    };

    Texture(CreateInfo const& info);

    Texture(Texture const&) = delete;
    Texture(Texture&&);

    Texture& operator=(Texture const&) = delete;
    Texture& operator=(Texture&&);

    ~Texture();

    void create(CreateInfo const& info);
    void destroy();

    vk::Image handle() const;
    ImageView view_handle() const;
    VmaAllocation memory_handle() const;

private:
    VulkanContext* ctx;
    RawImage image;
    ImageView view;
};


}

#endif
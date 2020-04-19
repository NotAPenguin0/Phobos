#ifndef PHOBOS_CUBEMAP_HPP_
#define PHOBOS_CUBEMAP_HPP_

#include <phobos/forward.hpp>
#include <phobos/util/image_util.hpp>

namespace ph {

class Cubemap {
public:
    Cubemap() = default;
    Cubemap(VulkanContext& ctx);

    struct CreateInfo {
        VulkanContext* ctx;
        size_t width;
        size_t height;
        std::array<uint8_t const*, 6> faces;
        vk::Format format = vk::Format::eR8G8B8Srgb;
        size_t channels = 4;
    };

    Cubemap(CreateInfo const& info);

    Cubemap(Cubemap const&) = delete;
    Cubemap(Cubemap&&);

    Cubemap& operator=(Cubemap const&) = delete;
    Cubemap& operator=(Cubemap&&);

    ~Cubemap();

    void create(CreateInfo const& info);
    void destroy();

    vk::Image handle() const;
    ImageView view_handle() const;
    VmaAllocation memory_handle() const;

private:
    VulkanContext* ctx = nullptr;
    RawImage image{};
    ImageView view{};
};

}

#endif
#pragma once

#include <functional>
#include <vector>

#include <vulkan/vulkan.h>

namespace ph {

// Credits for this hash_cobmine go to martty
template <typename T>
inline void hash_combine(size_t& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
template <typename T, typename... Rest>
inline void hash_combine(size_t& seed, const T& v, Rest const&... rest) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    hash_combine(seed, rest...);
}

template<typename E>
constexpr auto to_integral(E e) -> typename std::underlying_type<E>::type {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

}

namespace std {

template<typename T>
struct hash<vector<T>> {
    size_t operator()(::std::vector<T> const& v) const noexcept {
        size_t h = 0;
        for (auto const& elem : v) {
            ph::hash_combine(h, elem);
        }
        return h;
    }
};

template<>
struct hash<VkAttachmentDescription> {
    size_t operator()(VkAttachmentDescription const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.flags, x.initialLayout, x.finalLayout, x.format, x.loadOp,
            x.stencilLoadOp, x.storeOp, x.stencilStoreOp, x.samples);
        return h;
    }
};

template<>
struct hash<VkAttachmentReference> {
    size_t operator()(VkAttachmentReference const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.attachment, x.layout);
        return h;
    }
};

template<>
struct hash<VkSubpassDescription> {
    size_t operator()(VkSubpassDescription const& x) const noexcept {
        size_t h = 0;
        for (size_t i = 0; i < x.colorAttachmentCount; ++i) {
            ph::hash_combine(h, x.pColorAttachments[i]);
        }
        if (x.pDepthStencilAttachment) {
            ph::hash_combine(h, *x.pDepthStencilAttachment);
        }
        return h;
    }
};

template<>
struct hash<VkSubpassDependency> {
    size_t operator()(VkSubpassDependency const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.srcSubpass, x.dstSubpass,
            x.srcAccessMask, x.dstAccessMask,
           x.srcStageMask, x.dstStageMask);
        return h;
    }
};

template<>
struct hash<VkRenderPassCreateInfo> {
    size_t operator()(VkRenderPassCreateInfo const& info) const noexcept {
        size_t h = 0;
        for (size_t i = 0; i < info.attachmentCount; ++i) {
            ph::hash_combine(h, info.pAttachments[i]);
        }
        for (size_t i = 0; i < info.subpassCount; ++i) {
            ph::hash_combine(h, info.pSubpasses[i]);
        }
        for (size_t i = 0; i < info.dependencyCount; ++i) {
            ph::hash_combine(h, info.pDependencies[i]);
        }
        return h;
    }
};

template<>
struct hash<VkRenderPass> {
    size_t operator()(VkRenderPass const& pass) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, reinterpret_cast<uint64_t>(pass));
        return h;
    }
};

template<>
struct hash<VkImageView> {
    size_t operator()(VkImageView const& view) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, reinterpret_cast<uint64_t>(view));
        return h;
    }
};

template<>
struct hash<VkFramebufferCreateInfo> {
    size_t operator()(VkFramebufferCreateInfo const& info) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, info.renderPass, info.width, info.height, info.layers);
        for (size_t i = 0; i < info.attachmentCount; ++i) {
            ph::hash_combine(h, info.pAttachments[i]);
        }
        // Flags aren't hashed ...
        return h;
    }
};

}
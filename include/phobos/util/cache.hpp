#ifndef PHOBOS_UTIL_CACHE_HPP_
#define PHOBOS_UTIL_CACHE_HPP_

#include <functional>
#include <type_traits>
#include <unordered_map>
#include <optional>

#include <vulkan/vulkan.hpp>

#include <phobos/pipeline/pipeline.hpp>

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

// thanks martty, saves me some typing
// I did some myself
template <class BitType, class MaskType>
struct hash<vk::Flags<BitType, MaskType>> {
    size_t operator()(vk::Flags<BitType, MaskType> const& x) const noexcept {
        return std::hash<MaskType>()((MaskType)x);
    }
};

template<typename T>
struct hash<stl::vector<T>> {
    size_t operator()(stl::vector<T> const& v) const noexcept {
        size_t h = 0;
        for (auto const& elem : v) {
            ph::hash_combine(h, elem);
        }
        return h;
    }
};

template<typename T>
struct hash<::std::vector<T>> {
    size_t operator()(::std::vector<T> const& v) const noexcept {
        size_t h = 0;
        for (auto const& elem : v) {
            ph::hash_combine(h, elem);
        }
        return h;
    }
};

template<>
struct hash<vk::AttachmentDescription> {
    size_t operator()(vk::AttachmentDescription const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.flags, x.initialLayout, x.finalLayout, x.format, x.loadOp, 
                         x.stencilLoadOp, x.storeOp, x.stencilStoreOp, x.samples);
        return h;
    }
};

template<>
struct hash<vk::RenderPassCreateInfo> {
    size_t operator()(vk::RenderPassCreateInfo const& info) const noexcept {
        size_t h = 0;
        for (size_t i = 0; i < info.attachmentCount; ++i) {
            ph::hash_combine(h, info.pAttachments[i]);
        }
        return h;
    }   
};

template<>
struct hash<vk::RenderPass> {
    size_t operator()(vk::RenderPass const& pass) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, reinterpret_cast<uint64_t>(static_cast<VkRenderPass>(pass)));
        return h;
    }  
};

template<>
struct hash<vk::ImageView> {
    size_t operator()(vk::ImageView const& view) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, reinterpret_cast<uint64_t>(static_cast<VkImageView>(view)));
        return h;
    }
};

template<>
struct hash<vk::FramebufferCreateInfo> {
    size_t operator()(vk::FramebufferCreateInfo const& info) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, info.renderPass, info.width, info.height, info.layers);
        for (size_t i = 0; i < info.attachmentCount; ++i) {
            ph::hash_combine(h, info.pAttachments[i]);
        }
        // Flags aren't hashed ...
        return h;
    }
};

template<>
struct hash<vk::PipelineLayout> {
    size_t operator()(vk::PipelineLayout const& layout) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, reinterpret_cast<uint64_t>(static_cast<VkPipelineLayout>(layout)));
        return h;
    }
};

template<>
struct hash<vk::VertexInputAttributeDescription> {
    size_t operator()(vk::VertexInputAttributeDescription const& attr) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, attr.location, attr.binding, attr.format, attr.offset);
        return h;
    }
};

template<>
struct hash<vk::ShaderModule> {
    size_t operator()(vk::ShaderModule const& module) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, reinterpret_cast<uint64_t>(static_cast<VkShaderModule>(module)));
        return h;
    }
};

template<>
struct hash<ph::ShaderModuleCreateInfo> {
    size_t operator()(ph::ShaderModuleCreateInfo const& shader) const noexcept {
        size_t h = 0;
        // flags aren't hashed
        ph::hash_combine(h, shader.stage, shader.entry_point);
        if (shader.code_hash != 0) { ph::hash_combine(h, shader.code_hash); }
        else {
            // Expensive path if no precomputed hash is available
            ph::hash_combine(h, shader.code);
        }
        return h;
    }  
};

template <>
struct hash<vk::PipelineInputAssemblyStateCreateInfo> {
    size_t operator()(vk::PipelineInputAssemblyStateCreateInfo const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.flags, x.primitiveRestartEnable, ph::to_integral(x.topology));
        return h;
    }
};

template <>
struct hash<vk::StencilOpState> {
    size_t operator()(vk::StencilOpState const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.compareMask, ph::to_integral(x.compareOp), ph::to_integral(x.failOp), 
            ph::to_integral(x.depthFailOp), ph::to_integral(x.passOp), 
            x.reference, x.writeMask);
        return h;
    }
};


template <>
struct hash<vk::PipelineDepthStencilStateCreateInfo> {
    size_t operator()(vk::PipelineDepthStencilStateCreateInfo const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.flags, x.back, x.front, x.depthBoundsTestEnable, 
            ph::to_integral(x.depthCompareOp), x.depthTestEnable, x.depthWriteEnable, 
            x.maxDepthBounds, x.minDepthBounds, x.stencilTestEnable);
        return h;
    }
};

template <>
struct hash<vk::DynamicState> {
    size_t operator()(vk::DynamicState const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, ph::to_integral(x));
        return h;
    }
};

template<>
struct hash<vk::PipelineRasterizationStateCreateInfo> {
    size_t operator()(vk::PipelineRasterizationStateCreateInfo const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.flags, x.depthClampEnable, x.rasterizerDiscardEnable, ph::to_integral(x.polygonMode),
            x.cullMode, ph::to_integral(x.frontFace), x.depthBiasEnable,
            x.depthBiasConstantFactor, x.depthBiasClamp, x.depthBiasSlopeFactor, x.lineWidth);
        return h;
    }
};

template <>
struct hash<vk::PipelineMultisampleStateCreateInfo> {
    size_t operator()(vk::PipelineMultisampleStateCreateInfo const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.flags, x.alphaToCoverageEnable, x.alphaToOneEnable, x.minSampleShading, x.rasterizationSamples, x.sampleShadingEnable);
        if (x.pSampleMask) ph::hash_combine(h, *x.pSampleMask);
        return h;
    }
};

template <>
struct hash<vk::PipelineColorBlendAttachmentState> {
    size_t operator()(vk::PipelineColorBlendAttachmentState const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, ph::to_integral(x.alphaBlendOp), x.blendEnable, 
            ph::to_integral(x.colorBlendOp), ph::to_integral(x.dstAlphaBlendFactor), 
            ph::to_integral(x.srcAlphaBlendFactor), ph::to_integral(x.dstColorBlendFactor), 
            ph::to_integral(x.srcColorBlendFactor));
        return h;
    }
};

template <>
struct hash<vk::Extent2D> {
    size_t operator()(vk::Extent2D const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.width, x.height);
        return h;
    }
};

template <>
struct hash<vk::Extent3D> {
    size_t operator()(vk::Extent3D const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.width, x.height, x.depth);
        return h;
    }
};

template <>
struct hash<vk::Offset2D> {
    size_t operator()(vk::Offset2D const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.x, x.y);
        return h;
    }
};

template <>
struct hash<vk::Rect2D> {
    size_t operator()(vk::Rect2D const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.extent, x.offset);
        return h;
    }
};

template <>
struct hash<vk::Viewport> {
    size_t operator()(vk::Viewport const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.x, x.y, x.width, x.height, x.minDepth, x.maxDepth);
        return h;
    }
};

template <>
struct hash<vk::PipelineViewportStateCreateInfo> {
    size_t operator()(vk::PipelineViewportStateCreateInfo const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.flags);
        if (x.pScissors) {
            for (size_t i = 0; i < x.scissorCount; ++i) {
                ph::hash_combine(h, x.pScissors[i]);
            }
        }
        else ph::hash_combine(h, x.scissorCount);
        if (x.pViewports) { 
            for (size_t i = 0; i < x.viewportCount; ++i) {
                ph::hash_combine(h, x.pViewports[i]);
            }
        }
        else ph::hash_combine(h, x.viewportCount);
        return h;
    }
};

template<>
struct hash<vk::PushConstantRange> {
    size_t operator()(vk::PushConstantRange const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.size, x.offset, x.stageFlags);
        return h;
    }
};

template<>
struct hash<vk::Sampler> {
    size_t operator()(vk::Sampler const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, reinterpret_cast<uint64_t>(static_cast<VkSampler>(x)));
        return h;
    }
};

template<>
struct hash<ph::DescriptorImageInfo> {
    size_t operator()(ph::DescriptorImageInfo const& x) const noexcept {
        size_t h = 0;
        // Hash unique ID instead
        ph::hash_combine(h, x.view.id, x.sampler, ph::to_integral(x.layout));
        return h;
    }
};

template<>
struct hash<vk::Buffer> {
    size_t operator()(vk::Buffer const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, reinterpret_cast<uint64_t>((VkBuffer)x));
        return h;
    }
};

template<>
struct hash<ph::DescriptorBufferInfo> {
    size_t operator()(ph::DescriptorBufferInfo const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.buffer, (size_t)x.offset, (size_t)x.range);
        return h;
    }
};

template<>
struct hash<ph::DescriptorBinding> {
    size_t operator()(ph::DescriptorBinding const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.binding, ph::to_integral(x.type));
        for (auto const& d : x.descriptors) {
            if (x.type == vk::DescriptorType::eCombinedImageSampler 
                || x.type == vk::DescriptorType::eSampledImage) {
                ph::hash_combine(h, d.image); 
            } else { ph::hash_combine(h, d.buffer); }
        }
        return h;
    }
};

template<>
struct hash<ph::DescriptorSetBinding> {
    size_t operator()(ph::DescriptorSetBinding const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.bindings, x.set_layout);
        return h;
    }
};

template<>
struct hash<ph::ShaderHandle> {
    size_t operator()(ph::ShaderHandle const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.id);
        return h;
    }
};

template<>
struct hash<vk::DescriptorSetLayoutBinding> {
    size_t operator()(vk::DescriptorSetLayoutBinding const& x) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, x.binding, x.descriptorCount, x.descriptorType, x.stageFlags);
        // Make sure to only do this when there are immmutable samplers
        for (size_t i = 0; x.pImmutableSamplers && i < x.descriptorCount; ++i) {
            ph::hash_combine(h, x.pImmutableSamplers[i]);
        }
        return h;
    }
};

template<>
struct hash<ph::DescriptorSetLayoutCreateInfo> {
    size_t operator()(ph::DescriptorSetLayoutCreateInfo const& info) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, info.bindings, info.flags);
        return h;
    }
};

template<>
struct hash<ph::PipelineLayoutCreateInfo> {
    size_t operator()(ph::PipelineLayoutCreateInfo const& info) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, info.push_constants, info.set_layout);
        return h;
    }
};

template<>
struct hash<ph::PipelineCreateInfo> {
    size_t operator()(ph::PipelineCreateInfo const& info) const noexcept {
        size_t h = 0;
        ph::hash_combine(h, info.layout, info.vertex_attributes, info.shaders, 
            info.input_assembly, info.depth_stencil, info.dynamic_states,
            info.rasterizer, info.multisample, info.blend_attachments, info.blend_logic_op_enable,
            info.render_pass, info.subpass, info.viewports, info.scissors);
        // flags aren't hashed
        return h;
    }
};

} // namespace std

namespace ph {

template<typename T, typename LookupT>
class Cache {
public:
    struct Entry {
        T data;
        LookupT key;
        size_t frames_since_last_usage = 0;
        bool used_this_frame = false;
    };

    // Useful for cleanup
    auto& get_all() { return cache; }
    
    void insert(LookupT const& key, T&& val) {
        size_t hash = std::hash<LookupT>()(key);
        cache[hash] = Entry { stl::move(val), key, 0 };
    }

    void insert(LookupT&& key, T&& val) {
        size_t hash = std::hash<LookupT>()(key);
        cache[hash] = Entry{ stl::move(val), stl::move(key), 0 };
    }

    T* get(LookupT const& key) {
        size_t hash = std::hash<LookupT>()(key);
        auto it = cache.find(hash);
        if (it != cache.end()) {
            it->second.used_this_frame = true;
            return &it->second.data;
        }
        else return nullptr;
    }

    T const* get(LookupT const& key) const {
        size_t hash = std::hash<LookupT>()(key);
        auto it = cache.find(hash);
        if (it != cache.end()) { 
            // Mark the entry as used
            it->second.used_this_frame = true;
            return &it->second.data; 
        }
        else return nullptr;
    }

    LookupT* get_key(size_t hash) {
        return &cache.at(hash).key;
    }

    size_t get_frames_since_last_usage(LookupT const& key) {
        size_t hash = std::hash<LookupT>()(key);
        return cache[hash].frames_since_last_usage;
    }

private:
    std::unordered_map<size_t, Entry> cache;
};

}

#endif
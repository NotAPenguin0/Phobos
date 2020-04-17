#ifndef CACHE_CLEANUP_HPP_
#define CACHE_CLEANUP_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/pipeline/pipeline.hpp>

namespace ph {

// Functions for deallocation of all cached resources

void destroy(VulkanContext* ctx, vk::FramebufferCreateInfo const& fbci, vk::Framebuffer framebuf);
void destroy(VulkanContext* ctx, vk::RenderPassCreateInfo const& rpci, vk::RenderPass renderpass);
void destroy(VulkanContext* ctx, DescriptorSetLayoutCreateInfo const& dslci, vk::DescriptorSetLayout set_Layout);
void destroy(VulkanContext* ctx, PipelineLayoutCreateInfo const& plci, PipelineLayout layout);
void destroy(VulkanContext* ctx, PipelineCreateInfo const& pci, vk::Pipeline pipeline);
void destroy(VulkanContext* ctx, DescriptorSetBinding const& info, vk::DescriptorSet set);

template<typename T, typename LookupT>
void update_cache_resource_usage(VulkanContext* ctx, Cache<T, LookupT>& cache, size_t max_frames_in_flight) {
    auto& data = cache.get_all();
    for (auto it = data.begin(); it != data.end(); ) {
        size_t hash = it->first;
        auto& entry = it->second;
        // If a descriptor was used this frame, we reset frames_since_last_usage
        if (entry.used_this_frame) {
            entry.frames_since_last_usage = 0;
            entry.used_this_frame = false;
        }
        // Now, each used entry will have a frames_since_last_usage of 1, while each unused entry
        // will have its frames_since_last_usage incremented by 1.
        entry.frames_since_last_usage++;

        // If this entry has exceeded the max amount of frames since its last usage, free it.
        if (entry.frames_since_last_usage > max_frames_in_flight + 1) {
            destroy(ctx, entry.key, entry.data);
            // std::unordered_map guarantees iterator stability when erasing an element, so this is safe
            // to do in a loop
            it = data.erase(data.find(hash));
        } else {
            ++it;
        }
    }
}

} // namespace ph

#endif
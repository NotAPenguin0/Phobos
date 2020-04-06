#include <phobos/util/cache_cleanup.hpp>

namespace ph {

void destroy(VulkanContext* ctx, vk::FramebufferCreateInfo const&, vk::Framebuffer framebuf) {
    ctx->device.destroyFramebuffer(framebuf);
}

void destroy(VulkanContext* ctx, vk::RenderPassCreateInfo const&, vk::RenderPass renderpass) {
    ctx->device.destroyRenderPass(renderpass);
}

void destroy(VulkanContext* ctx, DescriptorSetLayoutCreateInfo const&, vk::DescriptorSetLayout set_layout) {
    ctx->device.destroyDescriptorSetLayout(set_layout);
}

void destroy(VulkanContext* ctx, PipelineLayoutCreateInfo const&, PipelineLayout layout) {
    ctx->device.destroyPipelineLayout(layout.layout);
}

void destroy(VulkanContext* ctx, PipelineCreateInfo const&, vk::Pipeline pipeline) {
    ctx->device.destroyPipeline(pipeline);
}

void destroy(VulkanContext* ctx, DescriptorSetBinding const& info, vk::DescriptorSet set) {
    ctx->device.freeDescriptorSets(info.pool, 1, &set);
}

}
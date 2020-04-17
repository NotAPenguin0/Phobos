#include <phobos/util/cache_cleanup.hpp>

#include <phobos/util/memory_util.hpp>

namespace ph {

void destroy(VulkanContext* ctx, vk::FramebufferCreateInfo const&, vk::Framebuffer framebuf) {
//    ctx->logger->write_fmt(log::Severity::Info, "Deallocating framebuffer {}.", (void*)memory_util::to_vk_type(framebuf));
    ctx->device.destroyFramebuffer(framebuf);
}

void destroy(VulkanContext* ctx, vk::RenderPassCreateInfo const&, vk::RenderPass renderpass) {
//    ctx->logger->write_fmt(log::Severity::Info, "Deallocating renderpass {}.", (void*)memory_util::to_vk_type(renderpass));
    ctx->device.destroyRenderPass(renderpass);
}

void destroy(VulkanContext* ctx, DescriptorSetLayoutCreateInfo const&, vk::DescriptorSetLayout set_layout) {
//    ctx->logger->write_fmt(log::Severity::Info, "Deallocating descriptor set layout {}.", (void*)memory_util::to_vk_type(set_layout));
    ctx->device.destroyDescriptorSetLayout(set_layout);
}

void destroy(VulkanContext* ctx, PipelineLayoutCreateInfo const&, PipelineLayout layout) {
//    ctx->logger->write_fmt(log::Severity::Info, "Deallocating pipeline layout {}.", (void*)memory_util::to_vk_type(layout.layout));
    ctx->device.destroyPipelineLayout(layout.layout);
}

void destroy(VulkanContext* ctx, PipelineCreateInfo const&, vk::Pipeline pipeline) {
//    ctx->logger->write_fmt(log::Severity::Info, "Deallocating pipeline {}.", (void*)memory_util::to_vk_type(pipeline));
    ctx->device.destroyPipeline(pipeline);
}

void destroy(VulkanContext* ctx, DescriptorSetBinding const& info, vk::DescriptorSet set) {
//    ctx->logger->write_fmt(log::Severity::Info, "Deallocating descriptor set {}.", (void*)memory_util::to_vk_type(set));
    ctx->device.freeDescriptorSets(info.pool, 1, &set);
}

}
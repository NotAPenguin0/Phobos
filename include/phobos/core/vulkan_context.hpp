#ifndef PHOBOS_VULKAN_CONTEXT_HPP_
#define PHOBOS_VULKAN_CONTEXT_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/version.hpp>
#include <phobos/core/window_context.hpp>
#include <phobos/core/physical_device.hpp>
#include <phobos/core/swapchain.hpp>

#include <phobos/pipeline/pipeline.hpp>

#include <phobos/util/log_interface.hpp>

#include <phobos/events/event_dispatcher.hpp>

#include <phobos/util/cache.hpp>

namespace ph {

struct VulkanContext {
    vk::Instance instance;    
    vk::DispatchLoaderDynamic dynamic_dispatcher;
    // Only available if enable_validation_layers is set to true
    vk::DebugUtilsMessengerEXT debug_messenger;
    PhysicalDeviceDetails physical_device;
    vk::Device device;

    WindowContext* window_ctx;

    vk::Queue graphics_queue;
    SwapchainDetails swapchain;
    PipelineManager pipelines;

    // A command pool that can be used to allocate various command buffers for various operations. 
    // This command pool is marked with the eTransient flag
    vk::CommandPool command_pool;
    
    log::LogInterface* logger = nullptr;
    EventDispatcher event_dispatcher;

    Cache<vk::RenderPass, vk::RenderPassCreateInfo> renderpass_cache;
    Cache<vk::Framebuffer, vk::FramebufferCreateInfo> framebuffer_cache;
    Cache<vk::Pipeline, PipelineCreateInfo> pipeline_cache;
    Cache<PipelineLayout, PipelineLayoutCreateInfo> pipeline_layout_cache;
    Cache<vk::DescriptorSetLayout, DescriptorSetLayoutCreateInfo> set_layout_cache;

    void destroy();
};

struct AppSettings {
    bool enable_validation_layers = false;
    Version version = Version {1, 0, 0};
};

VulkanContext* create_vulkan_context(WindowContext& window_ctx, log::LogInterface* logger, AppSettings settings = {});

}

#endif
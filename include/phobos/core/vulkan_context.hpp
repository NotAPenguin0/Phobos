#ifndef PHOBOS_VULKAN_CONTEXT_HPP_
#define PHOBOS_VULKAN_CONTEXT_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/version.hpp>
#include <phobos/core/window_context.hpp>
#include <phobos/core/physical_device.hpp>
#include <phobos/core/swapchain.hpp>

#include <phobos/pipeline/pipeline_layout.hpp>
#include <phobos/pipeline/pipeline.hpp>

#include <phobos/util/log_interface.hpp>

#include <phobos/events/event_dispatcher.hpp>

namespace ph {

struct VulkanContext : public EventListener<SwapchainRecreateEvent> {
    vk::Instance instance;    
    vk::DispatchLoaderDynamic dynamic_dispatcher;

    WindowContext* window_ctx;

    // Only available if enable_validation_layers is set to true
    vk::DebugUtilsMessengerEXT debug_messenger;

    PhysicalDeviceDetails physical_device;
    vk::Device device;

    vk::Queue graphics_queue;

    SwapchainDetails swapchain;

    PipelineLayouts pipeline_layouts;
    PipelineManager pipelines;

    vk::RenderPass default_render_pass;
    // Renderpass that only has a color attachment
    vk::RenderPass swapchain_render_pass;
    // A command pool that can be used to allocate various command buffers for various operations. 
    // This command pool is marked with the eTransient flag
    vk::CommandPool command_pool;

    log::LogInterface* logger = nullptr;

    EventDispatcher event_dispatcher;

    void destroy();

protected:
    void on_event(SwapchainRecreateEvent const& evt) override;
};

struct AppSettings {
    bool enable_validation_layers = false;
    Version version = Version {1, 0, 0};
};

VulkanContext* create_vulkan_context(WindowContext& window_ctx, log::LogInterface* logger, AppSettings settings = {});

}

#endif
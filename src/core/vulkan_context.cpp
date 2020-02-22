#include <phobos/core/vulkan_context.hpp>
#include <phobos/core/device.hpp>

#include <mimas/mimas.h>
#include <mimas/mimas_vk.h>
#include <vector>
#include <iostream>

#include <phobos/pipeline/pipelines.hpp>
#include <phobos/pipeline/layouts.hpp>

namespace ph {

static const char** get_required_window_extensions() {
    uint32_t count;
    const char** extensions = mimas_get_vk_extensions();

    return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT type,
    [[maybe_unused]] VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    [[maybe_unused]] void* user_data) {

    // Only log messages with severity 'warning' or above
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        VulkanContext* ctx = reinterpret_cast<VulkanContext*>(user_data);
        // TODO: Implement severity
        ctx->logger->write(ph::log::Severity::Warning, callback_data->pMessage);
    }

    return VK_FALSE;
}

static vk::Instance create_vulkan_instance(vk::ApplicationInfo const& app_info, AppSettings const& app_settings) {
    vk::InstanceCreateInfo info;
    info.pApplicationInfo = &app_info;

    const char** extensions_c = get_required_window_extensions();
    std::vector<const char*> extensions;
    const char** ptr = extensions_c;
    while(*ptr) {
        extensions.push_back(*ptr);
        ++ptr;
    }
    std::vector<const char*> const layers = {
        "VK_LAYER_KHRONOS_validation"
    };
    if (app_settings.enable_validation_layers) {
        // Add debug callback extensions if validation layers are enabled
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        info.enabledLayerCount = layers.size();
        info.ppEnabledLayerNames = layers.data();
    } else {
        info.enabledLayerCount = 0;
        info.ppEnabledLayerNames = nullptr;
    }

    info.enabledExtensionCount = extensions.size();
    info.ppEnabledExtensionNames = extensions.data();

    return vk::createInstance(info);
}

static vk::DebugUtilsMessengerEXT create_debug_messenger(VulkanContext* ctx) {
    vk::DebugUtilsMessengerCreateInfoEXT info;
    // Specify message severity and message types to log
    info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | 
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral;
    info.pfnUserCallback = &vk_debug_callback;  
    info.pUserData = ctx;
    
    return ctx->instance.createDebugUtilsMessengerEXT(info, nullptr, ctx->dynamic_dispatcher);
}

static vk::RenderPass create_default_render_pass(VulkanContext& ctx) {
    // Create attachment
    vk::AttachmentDescription color_attachment;
    color_attachment.format = ctx.swapchain.format.format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference color_attachment_ref;
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentDescription depth_attachment;
    depth_attachment.format = vk::Format::eD32Sfloat;
    depth_attachment.samples = vk::SampleCountFlagBits::e1;
    depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
    depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference depth_attachment_ref;
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    // Create subpass
    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    // Setup subpass dependencies
    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

    vk::AttachmentDescription attachments[2] = { color_attachment, depth_attachment };

    vk::RenderPassCreateInfo info;
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    return ctx.device.createRenderPass(info);
}

void VulkanContext::destroy() { 
    pipelines.destroy_all(device);
    pipeline_layouts.destroy_all(device);
    device.destroyRenderPass(default_render_pass);

    device.destroyImage(swapchain.depth_image);
    device.destroyImageView(swapchain.depth_image_view);
    device.freeMemory(swapchain.depth_image_memory);

    for (auto const& framebuf : swapchain.framebuffers) {
        device.destroyFramebuffer(framebuf);
    }

    for (auto const& img_view : swapchain.image_views) {
        device.destroyImageView(img_view);
    }
    
    device.destroyCommandPool(command_pool);

    device.destroySwapchainKHR(swapchain.handle);
    // Destroy the device
    device.destroy();
    // Destroy the surface
    instance.destroySurfaceKHR(physical_device.surface_details.handle);
    // Destroy the debug messenger
    instance.destroyDebugUtilsMessengerEXT(debug_messenger, nullptr, dynamic_dispatcher);
    // Finally, destroy the VkInstance
    instance.destroy();
}

VulkanContext* create_vulkan_context(WindowContext const& window_ctx, log::LogInterface* logger, AppSettings settings) {
    VulkanContext* context = new VulkanContext;

    vk::ApplicationInfo app_info;
    app_info.pApplicationName = window_ctx.title.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(settings.version.major, settings.version.minor, settings.version.patch);
    app_info.pEngineName = "Phobos Renderer";

    static constexpr Version version = PHOBOS_VERSION;
    app_info.engineVersion = VK_MAKE_VERSION(version.major, version.minor, version.patch);
    app_info.apiVersion = VK_VERSION_1_2;

    context->logger = logger;
    context->instance = create_vulkan_instance(app_info, settings);
    context->dynamic_dispatcher = { context->instance, vkGetInstanceProcAddr };

    if (settings.enable_validation_layers) {
        context->debug_messenger = create_debug_messenger(context);    
    }

    logger->write_fmt(log::Severity::Info, "Created VkInstance");

    vk::SurfaceKHR surface;

    // Create surface
    mimas_create_vk_surface(window_ctx.handle, context->instance, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface));

    PhysicalDeviceRequirements requirements;
    // Require swapchain extension
    requirements.required_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    context->physical_device = get_physical_device(context->instance, surface, requirements);

    logger->write_fmt(log::Severity::Info, "Picked physical device {}", context->physical_device.properties.deviceName);

    // Create logical device
    DeviceRequirements device_requirements;
    device_requirements.extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_requirements.features.samplerAnisotropy = true;
    vk::PhysicalDeviceDescriptorIndexingFeatures descriptor_indexing;
    descriptor_indexing.shaderSampledImageArrayNonUniformIndexing = true;
    descriptor_indexing.runtimeDescriptorArray = true;
    descriptor_indexing.descriptorBindingVariableDescriptorCount = true;
    descriptor_indexing.descriptorBindingPartiallyBound = true;
    device_requirements.pNext = &descriptor_indexing;
    context->device = create_device(context->physical_device, device_requirements);

    logger->write_fmt(log::Severity::Info, "Created VkDevice");

    // Finally, get the graphics queue
    context->graphics_queue = context->device.getQueue(context->physical_device.queue_families.graphics_family.value(), 0);

    context->swapchain = create_swapchain(context->device, window_ctx, context->physical_device);

    logger->write_fmt(log::Severity::Info, "Created swapchain. Dimensions are {}x{}", 
        context->swapchain.extent.width, context->swapchain.extent.height);

    // We have to create the renderpass before creating the pipeline. 
    context->default_render_pass = create_default_render_pass(*context);

    logger->write_fmt(log::Severity::Info, "Created renderpass");

    // Only after the renderpass as been created, we can create the swapchain framebuffers
    create_swapchain_framebuffers(*context, context->swapchain);

    logger->write_fmt(log::Severity::Info, "Created framebuffers");

    create_pipeline_layouts(context->device, context->pipeline_layouts);
    create_pipelines(*context, context->pipelines);

    logger->write_fmt(log::Severity::Info, "Created pipelines");

    vk::CommandPoolCreateInfo cmd_pool_info;
    cmd_pool_info.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cmd_pool_info.queueFamilyIndex = context->physical_device.queue_families.graphics_family.value();
    context->command_pool = context->device.createCommandPool(cmd_pool_info);

    logger->write_fmt(log::Severity::Info, "Created command pool");

    return context;
}

}
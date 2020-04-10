#include <phobos/core/vulkan_context.hpp>
#include <phobos/core/device.hpp>

#include <mimas/mimas.h>
#include <mimas/mimas_vk.h>
#include <vector>
#include <iostream>

#include <phobos/util/memory_util.hpp>

namespace ph {

static const char** get_required_window_extensions(mimas_i32* count) {
    const char** extensions = mimas_get_vk_extensions(count);

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

static void window_resize_callback(Mimas_Window* window, mimas_i32 w, mimas_i32 h, void* user_data) {
    VulkanContext* ctx = reinterpret_cast<VulkanContext*>(user_data);
    ctx->window_ctx->width = w;
    ctx->window_ctx->height = h;
    ctx->event_dispatcher.fire_event(WindowResizeEvent{ctx->window_ctx, w, h});
}

static vk::Instance create_vulkan_instance(vk::ApplicationInfo const& app_info, AppSettings const& app_settings) {
    vk::InstanceCreateInfo info;
    info.pApplicationInfo = &app_info;

    mimas_i32 extension_count;
    const char** extensions_c = get_required_window_extensions(&extension_count);
    std::vector<const char*> extensions(extensions_c, extensions_c + extension_count);
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


void VulkanContext::destroy() { 
    for (auto&[info, pass] : renderpass_cache.get_all()) {
        device.destroyRenderPass(pass.data);
    }
    renderpass_cache.get_all().clear();

    for (auto&[info, framebuf] : framebuffer_cache.get_all()) {
        device.destroyFramebuffer(framebuf.data);
    }
    framebuffer_cache.get_all().clear();
    
    for (auto&[hash, set_layout] : set_layout_cache.get_all()) {
        device.destroyDescriptorSetLayout(set_layout.data);
    }
    set_layout_cache.get_all().clear();

    for (auto&[hash, layout] : pipeline_layout_cache.get_all()) {
        device.destroyPipelineLayout(layout.data.layout);
    }
    pipeline_layout_cache.get_all().clear();

    // Destroy the actual pipelines
    for (auto&[info, pipeline] : pipeline_cache.get_all()) {
        device.destroyPipeline(pipeline.data);
    }
    pipeline_cache.get_all().clear();

    for (auto const& img_view : swapchain.image_views) {
        device.destroyImageView(img_view);
    }
    
    vmaDestroyAllocator(allocator);
    device.destroyCommandPool(command_pool);
    device.destroySwapchainKHR(swapchain.handle);
    device.destroy();
    instance.destroySurfaceKHR(physical_device.surface_details.handle);
    instance.destroyDebugUtilsMessengerEXT(debug_messenger, nullptr, dynamic_dispatcher);
    instance.destroy();
}

VulkanContext* create_vulkan_context(WindowContext& window_ctx, log::LogInterface* logger, AppSettings settings) {
    VulkanContext* context = new VulkanContext;
    context->window_ctx = &window_ctx;

    mimas_set_window_resize_callback(window_ctx.handle, window_resize_callback, context);

    vk::ApplicationInfo app_info;
    app_info.pApplicationName = window_ctx.title.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(settings.version.major, settings.version.minor, settings.version.patch);
    app_info.pEngineName = "Phobos Renderer";

    static constexpr Version version = PHOBOS_VERSION;
    app_info.engineVersion = VK_MAKE_VERSION(version.major, version.minor, version.patch);
    app_info.apiVersion = VK_API_VERSION_1_2;

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

    VmaAllocatorCreateInfo aci{};
    aci.device = context->device;
    aci.physicalDevice = context->physical_device.handle;
    aci.instance = context->instance;
    aci.vulkanApiVersion = VK_MAKE_VERSION(1, 2, 0);

    vmaCreateAllocator(&aci, &context->allocator);

    // Finally, get the graphics queue
    context->graphics_queue = context->device.getQueue(context->physical_device.queue_families.graphics_family.value(), 0);

    context->swapchain = create_swapchain(context->device, window_ctx, context->physical_device);

    logger->write_fmt(log::Severity::Info, "Created swapchain. Dimensions are {}x{}", 
        context->swapchain.extent.width, context->swapchain.extent.height);

    create_default_pipelines(*context, context->pipelines);

    logger->write_fmt(log::Severity::Info, "Created pipelines");

    vk::CommandPoolCreateInfo cmd_pool_info;
    cmd_pool_info.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cmd_pool_info.queueFamilyIndex = context->physical_device.queue_families.graphics_family.value();
    context->command_pool = context->device.createCommandPool(cmd_pool_info);
    vk::DebugUtilsObjectNameInfoEXT cmd_pool_name_info;
    cmd_pool_name_info.objectHandle = memory_util::vk_to_u64(context->command_pool);
    cmd_pool_name_info.objectType = vk::ObjectType::eCommandPool;
    cmd_pool_name_info.pObjectName = "Main command pool";

    logger->write_fmt(log::Severity::Info, "Created command pool");

    return context;
}

}
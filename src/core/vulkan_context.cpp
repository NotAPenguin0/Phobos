#include <phobos/core/vulkan_context.hpp>
#include <phobos/core/device.hpp>

#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>

#include <phobos/pipeline/pipelines.hpp>
#include <phobos/pipeline/layouts.hpp>

namespace ph {

static std::vector<const char*> get_required_glfw_extensions() {
    uint32_t count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);

    return std::vector<const char*>(extensions, extensions + count);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT type,
    [[maybe_unused]] VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    [[maybe_unused]] void* user_data) {

    // Only log messages with severity 'warning' or above
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layers: ";
        std::cerr << callback_data->pMessage << std::endl;
    }

    return VK_FALSE;
}

static vk::Instance create_vulkan_instance(vk::ApplicationInfo const& app_info, AppSettings const& app_settings) {
    vk::InstanceCreateInfo info;
    info.pApplicationInfo = &app_info;

    std::vector<const char*> extensions = get_required_glfw_extensions();
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

static vk::DebugUtilsMessengerEXT create_debug_messenger(vk::Instance instance, vk::DispatchLoaderDynamic& dispatcher) {
    vk::DebugUtilsMessengerCreateInfoEXT info;
    // Specify message severity and message types to log
    info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | 
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral;
    info.pfnUserCallback = &vk_debug_callback;  
    
    return instance.createDebugUtilsMessengerEXT(info, nullptr, dispatcher);
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
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference color_attachment_ref;
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;


    // Create subpass
    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    vk::RenderPassCreateInfo info;
    info.attachmentCount = 1;
    info.pAttachments = &color_attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;

    return ctx.device.createRenderPass(info);
}

void VulkanContext::destroy() { 
    pipelines.destroy_all(device);
    pipeline_layouts.destroy_all(device);
    device.destroyRenderPass(default_render_pass);

    for (auto const& img_view : swapchain.image_views) {
        device.destroyImageView(img_view);
    }

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

VulkanContext create_vulkan_context(WindowContext const& window_ctx, AppSettings settings) {
    VulkanContext context;

    vk::ApplicationInfo app_info;
    app_info.pApplicationName = window_ctx.title.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(settings.version.major, settings.version.minor, settings.version.patch);
    app_info.pEngineName = "Phobos Renderer";

    static constexpr Version version = PHOBOS_VERSION;
    app_info.engineVersion = VK_MAKE_VERSION(version.major, version.minor, version.patch);
    app_info.apiVersion = VK_VERSION_1_2;

    context.instance = create_vulkan_instance(app_info, settings);
    context.dynamic_dispatcher = { context.instance, vkGetInstanceProcAddr };

    if (settings.enable_validation_layers) {
        context.debug_messenger = create_debug_messenger(context.instance, context.dynamic_dispatcher);    
    }

    vk::SurfaceKHR surface;

    // Create surface
    glfwCreateWindowSurface(context.instance, window_ctx.handle, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface));

    PhysicalDeviceRequirements requirements;
    // Require swapchain extension
    requirements.required_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    context.physical_device = get_physical_device(context.instance, surface, requirements);

    // Create logical device
    DeviceRequirements device_requirements;
    device_requirements.extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    context.device = create_device(context.physical_device, device_requirements);

    // Finally, get the graphics queue
    context.graphics_queue = context.device.getQueue(context.physical_device.queue_families.graphics_family.value(), 0);

    context.swapchain = create_swapchain(context.device, window_ctx, context.physical_device);

    // We have to create the renderpass before creating the pipeline. 
    context.default_render_pass = create_default_render_pass(context);

    create_pipeline_layouts(context.device, context.pipeline_layouts);
    create_pipelines(context, context.pipelines);

    return context;
}

}
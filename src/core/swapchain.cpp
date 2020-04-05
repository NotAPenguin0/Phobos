#include <phobos/core/swapchain.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/util/memory_util.hpp>
#include <phobos/core/vulkan_context.hpp>
#include <stl/enumerate.hpp>

#include <limits>
#undef min
#undef max

namespace ph {

static vk::SurfaceFormatKHR choose_surface_format(SurfaceDetails const& surface_details) {
    for (auto const& fmt : surface_details.formats) {
        // This is a nice format to use
        if (fmt.format == vk::Format::eB8G8R8A8Snorm &&
            fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return fmt;
        }
    }
    // If our preferred format isn't found, just return the first one for now
    return surface_details.formats[0];
}

static vk::PresentModeKHR choose_present_mode(SurfaceDetails const& surface_details) {
    // Mailbox is essentially triple buffering
    for (auto const& mode : surface_details.present_modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }

    // This is VSync
    return vk::PresentModeKHR::eFifo;
}

static vk::Extent2D choose_swapchain_extent(WindowContext const& window_ctx, SurfaceDetails const& surface_details) {
    if (surface_details.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return surface_details.capabilities.currentExtent;
    } else {
        // Clamp the extent to be within the allowed range
        vk::Extent2D extent = vk::Extent2D(window_ctx.width, window_ctx.height);
        extent.width = std::clamp(extent.width, surface_details.capabilities.minImageExtent.width, 
                                  surface_details.capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, surface_details.capabilities.minImageExtent.height, 
                                  surface_details.capabilities.maxImageExtent.height);
        return extent;
    }
}

SwapchainDetails create_swapchain(vk::Device device, WindowContext const& window_ctx, 
    PhysicalDeviceDetails& physical_device, vk::SwapchainKHR old_swapchain) {
    SwapchainDetails details;

    physical_device.surface_details.capabilities = physical_device.handle.getSurfaceCapabilitiesKHR(
            physical_device.surface_details.handle);
    physical_device.surface_details.formats = physical_device.handle.getSurfaceFormatsKHR(
        physical_device.surface_details.handle);
    physical_device.surface_details.present_modes = physical_device.handle.getSurfacePresentModesKHR(
        physical_device.surface_details.handle);

    details.format = choose_surface_format(physical_device.surface_details);
    details.present_mode = choose_present_mode(physical_device.surface_details);
    details.extent = choose_swapchain_extent(window_ctx, physical_device.surface_details);

    uint32_t image_count = physical_device.surface_details.capabilities.minImageCount + 1;
    // Make sure not to exceed maximum. A value of 0 means there is no maximum
    if (physical_device.surface_details.capabilities.maxImageCount != 0) {
        if (image_count > physical_device.surface_details.capabilities.maxImageCount) {
            image_count = physical_device.surface_details.capabilities.maxImageCount;
        }
    }

    vk::SwapchainCreateInfoKHR info;
    info.oldSwapchain = old_swapchain;
    info.surface = physical_device.surface_details.handle;
    info.minImageCount = image_count;
    info.imageFormat = details.format.format;
    info.imageColorSpace = details.format.colorSpace;
    info.imageExtent = details.extent;
    info.imageArrayLayers = 1;
    info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    // Since we only use one family (the graphics queue), we don't actually have to specify this
    // uint32_t queue_families[] = { physical_device.queue_families.graphics_family.value() };
    // info.pQueueFamilyIndices = queue_families;
    // info.queueFamilyIndexCount = 1;
    
    // Only a single queue will access the swapchain
    info.imageSharingMode = vk::SharingMode::eExclusive;
    info.preTransform = physical_device.surface_details.capabilities.currentTransform;
    info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    info.clipped = true;

    details.handle = device.createSwapchainKHR(info);
    
    details.images = device.getSwapchainImagesKHR(details.handle);
    details.image_views.resize(details.images.size());
    for (auto[index, view] : stl::enumerate(details.image_views.begin(), details.image_views.end())) {
        view = create_image_view(device, details.images[index], details.format.format);    
    }

    return details;
}

}
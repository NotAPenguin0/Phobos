#ifndef PHOBOS_SWAPCHAIN_HPP_
#define PHOBOS_SWAPCHAIN_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/window_context.hpp>
#include <phobos/physical_device.hpp>

namespace ph {

struct SwapchainDetails {
    vk::SwapchainKHR handle;

    vk::SurfaceFormatKHR format;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;

    std::vector<vk::Image> images;
    std::vector<vk::ImageView> image_views;
};

SwapchainDetails create_swapchain(vk::Device device, WindowContext const& window_ctx, PhysicalDeviceDetails const& physical_device);

}

#endif
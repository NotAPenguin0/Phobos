#ifndef PHOBOS_SWAPCHAIN_HPP_
#define PHOBOS_SWAPCHAIN_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/forward.hpp>
#include <phobos/core/window_context.hpp>
#include <phobos/core/physical_device.hpp>

#include <stl/vector.hpp>

namespace ph {

struct SwapchainDetails {
    vk::SwapchainKHR handle;

    vk::SurfaceFormatKHR format;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;

    std::vector<vk::Image> images;
    stl::vector<vk::ImageView> image_views;
};

SwapchainDetails create_swapchain(vk::Device device, WindowContext const& window_ctx, 
    PhysicalDeviceDetails& physical_device, vk::SwapchainKHR old_swapchain = nullptr);
    
}

#endif
#ifndef PHOBOS_SWAPCHAIN_HPP_
#define PHOBOS_SWAPCHAIN_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/forward.hpp>
#include <phobos/core/window_context.hpp>
#include <phobos/core/physical_device.hpp>

namespace ph {

struct SwapchainDetails {
    vk::SwapchainKHR handle;

    vk::SurfaceFormatKHR format;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;

    std::vector<vk::Image> images;
    std::vector<vk::ImageView> image_views;

    std::vector<vk::Framebuffer> framebuffers;

    vk::Image depth_image;
    vk::DeviceMemory depth_image_memory;
    vk::ImageView depth_image_view;
};

SwapchainDetails create_swapchain(vk::Device device, WindowContext const& window_ctx, PhysicalDeviceDetails const& physical_device);
void create_swapchain_framebuffers(VulkanContext& ctx, SwapchainDetails& swapchain);

}

#endif
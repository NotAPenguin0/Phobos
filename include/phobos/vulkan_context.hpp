#ifndef PHOBOS_VULKAN_CONTEXT_HPP_
#define PHOBOS_VULKAN_CONTEXT_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/window_context.hpp>

namespace ph {

struct VulkanContext {
    vk::Instance instance;

    ~VulkanContext();
};

VulkanContext create_vulkan_context(WindowContext const& window_ctx);

}

#endif
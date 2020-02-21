#ifndef PHOBOS_COMMAND_BUFFER_UTIL_HPP_
#define PHOBOS_COMMAND_BUFFER_UTIL_HPP_

#include <phobos/core/vulkan_context.hpp>

namespace ph {

vk::CommandBuffer begin_single_time_command_buffer(VulkanContext& ctx);
void end_single_time_command_buffer(VulkanContext& ctx, vk::CommandBuffer cmd_buffer);

}

#endif
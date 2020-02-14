#ifndef PHOBOS_PRESENT_FRAME_INFO_HPP_
#define PHOBOS_PRESENT_FRAME_INFO_HPP_

#include <vulkan/vulkan.hpp>
#include <vector>

namespace ph {

struct FrameInfo {
    size_t frame_index;
    size_t image_index;

    size_t draw_calls;

    vk::CommandBuffer command_buffer;
    vk::Framebuffer framebuffer;
    vk::Image image;

    vk::Fence fence;

    vk::Semaphore image_available;
    vk::Semaphore render_finished;

    std::vector<vk::CommandBuffer> extra_command_buffers;
};

}

#endif
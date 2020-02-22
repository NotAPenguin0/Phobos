#ifndef PHOBOS_PRESENT_MANAGER_HPP_
#define PHOBOS_PRESENT_MANAGER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>

#include <vector>


namespace ph {

class PresentManager {
public:
    PresentManager(VulkanContext& ctx, size_t max_frames_in_flight = 2);

    // Gets data for the current frame, like the framebuffer to use, command buffers, etc
    FrameInfo& get_frame_info();

    // Presents the frame 
    void present_frame(FrameInfo& frame);

    // Waits until a new frame is available for submitting
    void wait_for_available_frame();

    void destroy();

private:
    VulkanContext& context;

    size_t max_frames_in_flight;
    size_t frame_index = 0;
    uint32_t image_index = 0;
    std::vector<vk::Fence> image_in_flight_fences;
    std::vector<FrameInfo> frames;

    vk::CommandPool command_pool;
    std::vector<vk::CommandBuffer> command_buffers;

    // Pool for all "fixed" descriptors
    vk::DescriptorPool fixed_descriptor_pool;

    // Samplers
    vk::Sampler default_sampler;
};

}

#endif
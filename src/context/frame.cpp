// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Queue management
//		- Frame management
//		- Presenting

#include <phobos/impl/frame.hpp>
#include <phobos/impl/attachment.hpp>
#include <phobos/impl/context.hpp>
#include <phobos/impl/cache.hpp>

#include <cassert>

namespace ph {
namespace impl {

FrameImpl::FrameImpl(ContextImpl& ctx, AttachmentImpl& attachment_impl, CacheImpl& cache, AppSettings settings) :
	ctx(&ctx),
	attachment_impl(&attachment_impl),
	cache(&cache),
	in_flight_frames(settings.max_frames_in_flight) {

	per_frame = RingBuffer<PerFrame>(max_frames_in_flight());
	for (size_t i = 0; i < max_frames_in_flight(); ++i) {
		VkFence fence = nullptr;
		VkFenceCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCreateFence(ctx.device, &info, nullptr, &fence);

		VkSemaphore semaphore = nullptr;
		VkSemaphore semaphore2 = nullptr;
		VkSemaphoreCreateInfo sem_info{};
		sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		vkCreateSemaphore(ctx.device, &sem_info, nullptr, &semaphore);
		vkCreateSemaphore(ctx.device, &sem_info, nullptr, &semaphore2);

		per_frame.set(i, PerFrame{ .fence = fence, .gpu_finished = semaphore, .image_ready = semaphore2 });
	}
}

FrameImpl::~FrameImpl() {
	for (PerFrame& frame : per_frame) {
		vkDestroyFence(ctx->device, frame.fence, nullptr);
		vkDestroySemaphore(ctx->device, frame.gpu_finished, nullptr);
		vkDestroySemaphore(ctx->device, frame.image_ready, nullptr);
	}
}

uint32_t FrameImpl::max_frames_in_flight() const {
	return in_flight_frames;
}

void FrameImpl::wait_for_frame() {
	// Wait for an available frame context
	PerFrame& frame_data = per_frame.current();
	vkWaitForFences(ctx->device, 1, &frame_data.fence, true, std::numeric_limits<uint64_t>::max());

	// Get an image index to present to
	vkAcquireNextImageKHR(ctx->device, ctx->swapchain->handle, std::numeric_limits<uint64_t>::max(), frame_data.image_ready, nullptr, &ctx->swapchain->image_index);

	// Wait until the image is absolutely no longer in use. This can happen when there are more frames in flight than swapchain images, or when
	// vkAcquireNextImageKHR returns indices out of order
	if (ctx->swapchain->per_image[ctx->swapchain->image_index].fence != nullptr) {
		vkWaitForFences(ctx->device, 1, &ctx->swapchain->per_image[ctx->swapchain->image_index].fence, true, std::numeric_limits<uint64_t>::max());
	}
	// Mark this image as in use by the current frame
	ctx->swapchain->per_image[ctx->swapchain->image_index].fence = frame_data.fence;

	// Once we have a frame we need to update where the swapchain attachment in our attachments list is pointing to
	attachment_impl->update_swapchain_attachment(ctx->swapchain->per_image[ctx->swapchain->image_index].view);
}

void FrameImpl::submit_frame_commands(Queue& queue, CommandBuffer& cmd_buf) {
	PerFrame& frame_data = per_frame.current();

	// Reset our fence from last time so we can use it again now
	vkResetFences(ctx->device, 1, &frame_data.fence);

	queue.submit(cmd_buf, frame_data.fence, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, frame_data.image_ready, frame_data.gpu_finished);
}

void FrameImpl::present(Queue& queue) {
	assert(!ctx->is_headless() && "Tried presenting from a headless context.\n");

	PerFrame& frame_data = per_frame.current();

	// Present to chosen queue
	VkPresentInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.swapchainCount = 1;
	info.pSwapchains = &ctx->swapchain->handle;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &frame_data.gpu_finished;
	info.pImageIndices = &ctx->swapchain->image_index;
	queue.present(info);

	// Advance per-frame ringbuffers to the next element
	next_frame();
}

void FrameImpl::next_frame() {
	per_frame.next();
	ctx->next_frame();
	cache->next_frame();
}

}
}
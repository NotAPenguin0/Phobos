#pragma once

#include <phobos/context.hpp>

namespace ph {
namespace impl {

// This implementation may assume that the context is not headless, since it won't be created if it is.
class FrameImpl {
public:
	FrameImpl(ContextImpl& ctx, AttachmentImpl& attachment_impl, CacheImpl& cache, AppSettings settings);
	~FrameImpl();

	uint32_t max_frames_in_flight() const;
	[[nodiscard]] InFlightContext wait_for_frame();
	void submit_frame_commands(Queue& queue, CommandBuffer& cmd_buf);
	void present(Queue& queue);

	void next_frame();

	// Creates per-frame data. Needs the queues to be created so we need a post-init function for this.
	void post_init(Context& ctx, AppSettings const& settings);

private:
	ContextImpl* ctx;
	AttachmentImpl* attachment_impl;
	CacheImpl* cache;
	uint32_t const in_flight_frames = 0;

	struct PerFrame {
		VkFence fence = nullptr;
		VkSemaphore gpu_finished = nullptr;
		VkSemaphore image_ready = nullptr;

		ph::CommandBuffer cmd_buf;
		ph::ScratchAllocator vbo_allocator;
		ph::ScratchAllocator ibo_allocator;
		ph::ScratchAllocator ubo_allocator;
		ph::ScratchAllocator ssbo_allocator;
	};

	RingBuffer<PerFrame> per_frame{}; // RingBuffer with in_flight_frames elements.
};

}
}
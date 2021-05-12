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
	void wait_for_frame();
	void submit_frame_commands(Queue& queue, CommandBuffer& cmd_buf);
	void present(Queue& queue);

	void next_frame();

private:
	ContextImpl* ctx;
	AttachmentImpl* attachment_impl;
	CacheImpl* cache;
	uint32_t const in_flight_frames = 0;

	struct PerFrame {
		VkFence fence = nullptr;
		VkSemaphore gpu_finished = nullptr;
		VkSemaphore image_ready = nullptr;
	};

	RingBuffer<PerFrame> per_frame{}; // RingBuffer with in_flight_frames elements.
};

}
}
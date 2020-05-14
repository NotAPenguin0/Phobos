#include <phobos/renderer/cubemap.hpp>

#include <stl/assert.hpp>
#include <phobos/core/vulkan_context.hpp>

namespace ph {

Cubemap::Cubemap(VulkanContext& ctx) : ctx(&ctx) {

}

Cubemap::Cubemap(CreateInfo const& info) {
	create(info);
}

Cubemap::Cubemap(Cubemap&& rhs) {
	ctx = rhs.ctx;
	image = rhs.image;
	view = rhs.view;

	rhs.ctx = nullptr;
	rhs.image = {};
	rhs.view = {};
}

Cubemap& Cubemap::operator=(Cubemap&& rhs) {
	if (this != &rhs) {
		ctx = rhs.ctx;
		image = rhs.image;
		view = rhs.view;

		rhs.ctx = nullptr;
		rhs.image = {};
		rhs.view = {};
	}
	return *this;
}

Cubemap::~Cubemap() { destroy(); }

void Cubemap::create(CreateInfo const& info) {
	destroy();
	ctx = info.ctx;

	vk::DeviceSize const face_size = info.width * info.height * info.channels;
	vk::DeviceSize const size = face_size * info.faces.size();
	RawBuffer staging_buffer = create_buffer(*ctx, size, BufferType::TransferBuffer);
	std::byte* data_ptr = map_memory(*ctx, staging_buffer);

	std::array<BufferSlice, 6> face_slices;

	for (size_t i = 0; i < info.faces.size(); ++i) {
		uint8_t const* face_data = info.faces[i];
		STL_ASSERT(face_data, "face data may not be nullptr");
		vk::DeviceSize const offset = i * face_size;
		memcpy(data_ptr + offset, face_data, face_size);

		BufferSlice slice;
		slice.buffer = staging_buffer.buffer;
		slice.offset = offset;
		slice.range = face_size;
		face_slices[i] = slice;
	}

	flush_memory(*ctx, staging_buffer, 0, VK_WHOLE_SIZE);
	unmap_memory(*ctx, staging_buffer);

	image = create_image(*ctx, info.width, info.height, ImageType::Cubemap, info.format, info.faces.size());

	vk::CommandBuffer cmd_buf = ctx->graphics->begin_single_time(0);
	// Transition image layout to TransferDst so we can start fillig the image with data
	transition_image_layout(cmd_buf, image, vk::ImageLayout::eTransferDstOptimal);
	copy_buffer_to_image(cmd_buf, face_slices, image);
	// Transition image layout to ShaderReadOnlyOptimal so we can start sampling from it
	transition_image_layout(cmd_buf, image, vk::ImageLayout::eShaderReadOnlyOptimal);
	ctx->graphics->end_single_time(cmd_buf);
	ctx->device.waitIdle();
	ctx->graphics->free_single_time(cmd_buf, 0);

	destroy_buffer(*ctx, staging_buffer);

	view = create_image_view(ctx->device, image);
}

void Cubemap::destroy() {
	if (ctx && image.image) {
		destroy_image_view(*ctx, view);
		destroy_image(*ctx, image);
	}
}

vk::Image Cubemap::handle() const {
	return image.image;
}

ImageView Cubemap::view_handle() const {
	return view;
}

VmaAllocation Cubemap::memory_handle() const {
	return image.memory;
}



}
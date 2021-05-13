// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Core helper functions

#include <phobos/context.hpp>
#include <phobos/impl/context.hpp>
#include <phobos/impl/frame.hpp>
#include <phobos/impl/attachment.hpp>
#include <phobos/impl/pipeline.hpp>
#include <phobos/impl/cache.hpp>
#include <phobos/impl/image.hpp>
#include <phobos/impl/buffer.hpp>

namespace ph {

Context::Context(AppSettings settings) {
	context_impl = std::make_unique<impl::ContextImpl>(settings);
	image_impl = std::make_unique<impl::ImageImpl>(*context_impl);
	buffer_impl = std::make_unique<impl::BufferImpl>(*context_impl);
	cache_impl = std::make_unique<impl::CacheImpl>(*this, settings);
	attachment_impl = std::make_unique<impl::AttachmentImpl>(*context_impl, *image_impl);
	if (!is_headless()) {
		frame_impl = std::make_unique<impl::FrameImpl>(*context_impl, *attachment_impl, *cache_impl, settings);
	}
	context_impl->post_init(*this, *image_impl, settings);
	frame_impl->post_init(*this, settings);
	pipeline_impl = std::make_unique<impl::PipelineImpl>(*context_impl, *cache_impl);
}

Context::~Context() {
	// Reverse destruction order should take care of proper destruction of implementation classes.
	// Make sure to first wait for completion of all commands.
	vkDeviceWaitIdle(device());
}

// CORE CONTEXT

bool Context::is_headless() const {
	return context_impl->is_headless();
}

bool Context::validation_enabled() const {
	return context_impl->validation_enabled();
}

uint32_t Context::thread_count() const {
	return context_impl->thread_count();
}

Queue* Context::get_queue(QueueType type) {
	return context_impl->get_queue(type);
}

Queue* Context::get_present_queue() {
	return context_impl->get_present_queue();
}

void Context::name_object(ph::Pipeline const& pipeline, std::string const& name) {
	context_impl->name_object(pipeline, name);
}

void Context::name_object(VkRenderPass pass, std::string const& name) {
	context_impl->name_object(pass, name);
}

void Context::name_object(VkFramebuffer framebuf, std::string const& name) {
	context_impl->name_object(framebuf, name);
}

// FRAME

size_t Context::max_frames_in_flight() const {
	return frame_impl->max_frames_in_flight();
}

[[nodiscard]] InFlightContext Context::wait_for_frame() {
	return frame_impl->wait_for_frame();
}

void Context::submit_frame_commands(Queue& queue, CommandBuffer& cmd_buf) {
	frame_impl->submit_frame_commands(queue, cmd_buf);
}

void Context::present(Queue& queue) {
	frame_impl->present(queue);
}

// ATTACHMENT

Attachment* Context::get_attachment(std::string_view name) {
	return attachment_impl->get_attachment(name);
}

void Context::create_attachment(std::string_view name, VkExtent2D size, VkFormat format) {
	attachment_impl->create_attachment(name, size, format);
}

bool Context::is_swapchain_attachment(std::string const& name) {
	return attachment_impl->is_swapchain_attachment(name);
}

std::string Context::get_swapchain_attachment_name() const {
	return attachment_impl->get_swapchain_attachment_name();
}

// PIPELINE

ShaderHandle Context::create_shader(std::string_view path, std::string_view entry_point, PipelineStage stage) {
	return pipeline_impl->create_shader(path, entry_point, stage);
}

void Context::reflect_shaders(ph::PipelineCreateInfo& pci) {
	pipeline_impl->reflect_shaders(pci);
}

void Context::create_named_pipeline(ph::PipelineCreateInfo pci) {
	pipeline_impl->create_named_pipeline(std::move(pci));
}

ShaderMeta const& Context::get_shader_meta(std::string_view pipeline_name) {
	return pipeline_impl->get_shader_meta(pipeline_name);
}

VkSampler Context::basic_sampler() {
	return pipeline_impl->basic_sampler;
}

// IMAGE 

RawImage Context::create_image(ImageType type, VkExtent2D size, VkFormat format) {
	return image_impl->create_image(type, size, format);
}

void Context::destroy_image(RawImage& image) {
	return image_impl->destroy_image(image);
}

ImageView Context::create_image_view(RawImage const& target, ImageAspect aspect) {
	return image_impl->create_image_view(target, aspect);
}

void Context::destroy_image_view(ImageView& view) {
	return image_impl->destroy_image_view(view);
}

// BUFFER


RawBuffer Context::create_buffer(BufferType type, VkDeviceSize size) {
	return buffer_impl->create_buffer(type, size);
}

void Context::destroy_buffer(RawBuffer& buffer) {
	buffer_impl->destroy_buffer(buffer);
}

bool Context::is_valid_buffer(RawBuffer const& buffer) {
	return buffer_impl->is_valid_buffer(buffer);
}

bool Context::has_persistent_mapping(RawBuffer const& buffer) {
	return buffer_impl->has_persistent_mapping(buffer);
}

std::byte* Context::map_memory(RawBuffer& buffer) {
	return buffer_impl->map_memory(buffer);
}

void Context::flush_memory(BufferSlice slice) {
	buffer_impl->flush_memory(slice);
}

void Context::unmap_memory(RawBuffer& buffer) {
	buffer_impl->unmap_memory(buffer);
}

bool Context::ensure_buffer_size(RawBuffer& buf, VkDeviceSize requested_size) {
	return buffer_impl->ensure_buffer_size(buf, requested_size);
}

// CACHING

VkFramebuffer Context::get_or_create(VkFramebufferCreateInfo const& info, std::string const& name) {
	return cache_impl->get_or_create_framebuffer(info, name);
}

VkRenderPass Context::get_or_create(VkRenderPassCreateInfo const& info, std::string const& name) {
	return cache_impl->get_or_create_renderpass(info, name);
}

VkDescriptorSetLayout Context::get_or_create(DescriptorSetLayoutCreateInfo const& dslci) {
	return cache_impl->get_or_create_descriptor_set_layout(dslci);
}

PipelineLayout Context::get_or_create(PipelineLayoutCreateInfo const& plci, VkDescriptorSetLayout set_layout) {
	return cache_impl->get_or_create_pipeline_layout(plci, set_layout);
}

Pipeline Context::get_or_create(std::string_view name, VkRenderPass render_pass) {
	return cache_impl->get_or_create_pipeline(pipeline_impl->get_pipeline(name), render_pass);
}

VkDescriptorSet Context::get_or_create(DescriptorSetBinding set_binding, Pipeline const& pipeline, void* pNext) {
	return cache_impl->get_or_create_descriptor_set(set_binding, pipeline, pNext);
}


VkDevice Context::device() {
	return context_impl->device;
}

}
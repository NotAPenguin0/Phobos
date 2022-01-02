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
#include <plib/macros.hpp>

#if PHOBOS_ENABLE_RAY_TRACING
#include <phobos/acceleration_structure.hpp>
#endif

#include <cassert>

namespace ph {

Context::Context(AppSettings const& settings) {
	context_impl = std::make_unique<impl::ContextImpl>(settings);
	image_impl = std::make_unique<impl::ImageImpl>(*context_impl);
	buffer_impl = std::make_unique<impl::BufferImpl>(*context_impl);
	cache_impl = std::make_unique<impl::CacheImpl>(*this, settings);
	attachment_impl = std::make_unique<impl::AttachmentImpl>(*context_impl, *image_impl);
	if (!is_headless()) {
		frame_impl = std::make_unique<impl::FrameImpl>(*context_impl, *attachment_impl, *cache_impl, settings);
	}
	context_impl->post_init(*this, *image_impl, settings);
    if (!is_headless()) {
        frame_impl->post_init(*this, settings);
    }
	pipeline_impl = std::make_unique<impl::PipelineImpl>(*context_impl, *cache_impl, *buffer_impl);

#if PHOBOS_ENABLE_RAY_TRACING

#define PH_RTX_LOAD_FUNCTION(name) rtx_fun._##name = (PFN_##name)vkGetInstanceProcAddr(context_impl->instance, #name)
	PH_RTX_LOAD_FUNCTION(vkCreateAccelerationStructureKHR);
	PH_RTX_LOAD_FUNCTION(vkDestroyAccelerationStructureKHR);
	PH_RTX_LOAD_FUNCTION(vkGetAccelerationStructureDeviceAddressKHR);
	PH_RTX_LOAD_FUNCTION(vkGetAccelerationStructureBuildSizesKHR);
	PH_RTX_LOAD_FUNCTION(vkCmdBuildAccelerationStructuresKHR);
	PH_RTX_LOAD_FUNCTION(vkCmdWriteAccelerationStructuresPropertiesKHR);
	PH_RTX_LOAD_FUNCTION(vkCmdCopyAccelerationStructureKHR);
	PH_RTX_LOAD_FUNCTION(vkCreateRayTracingPipelinesKHR);
	PH_RTX_LOAD_FUNCTION(vkCmdTraceRaysKHR);
#undef PH_RTX_LOAD_FUNCTION
#endif
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

PhysicalDevice const& Context::get_physical_device() const {
	return context_impl->phys_device;
}

Queue* Context::get_queue(QueueType type) {
	return context_impl->get_queue(type);
}

Queue* Context::get_present_queue() {
	return context_impl->get_present_queue();
}

void Context::wait_idle() {
	vkDeviceWaitIdle(device());
}

LogInterface* Context::logger() {
	return context_impl->get_logger();
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

void Context::name_object(VkBuffer buffer, std::string const& name) {
	context_impl->name_object(buffer, name);
}

void Context::name_object(VkImage image, std::string const& name) {
	context_impl->name_object(image, name);
}

void Context::name_object(ph::RawImage const& image, std::string const& name) {
	context_impl->name_object(image, name);
}

void Context::name_object(ph::ImageView const& view, std::string const& name) {
	context_impl->name_object(view, name);
}

void Context::name_object(VkFence fence, std::string const& name) {
	context_impl->name_object(fence, name);
}

void Context::name_object(VkSemaphore semaphore, std::string const& name) {
	context_impl->name_object(semaphore, name);
}

void Context::name_object(VkCommandPool pool, std::string const& name) {
	context_impl->name_object(pool, name);
}

void Context::name_object(ph::CommandBuffer const& cmd_buf, std::string const& name) {
	context_impl->name_object(cmd_buf, name);
}

void Context::name_object(VkQueue queue, std::string const& name) {
	context_impl->name_object(queue, name);
}

[[nodiscard]] InThreadContext Context::begin_thread(uint32_t thread_index) {
	return context_impl->begin_thread(thread_index);
}
void Context::end_thread(uint32_t thread_index) {
	context_impl->end_thread(thread_index);
}

VkFence Context::create_fence() {
	return context_impl->create_fence();
}

void Context::wait_for_fence(VkFence fence, uint64_t timeout) {
	context_impl->wait_for_fence(fence, timeout);
}

bool Context::poll_fence(VkFence fence) {
	// Timeout value of zero will poll the fence. In this case vkWaitForFences will return VK_SUCCESS if the fence has been
	// signalled, and VK_TIMEOUT if not.
	return context_impl->wait_for_fence(fence, 0) == VK_SUCCESS;
}

void Context::reset_fence(VkFence fence) {
	return context_impl->reset_fence(fence);
}

void Context::destroy_fence(VkFence fence) {
	context_impl->destroy_fence(fence);
}

VkSemaphore Context::create_semaphore() {
	return context_impl->create_semaphore();
}

void Context::destroy_semaphore(VkSemaphore semaphore) {
	context_impl->destroy_semaphore(semaphore);
}

VkSampler Context::create_sampler(VkSamplerCreateInfo info) {
	return context_impl->create_sampler(info);
}

void Context::destroy_sampler(VkSampler sampler) {
	context_impl->destroy_sampler(sampler);
}

VkQueryPool Context::create_query_pool(VkQueryType type, uint32_t count) {
	return context_impl->create_query_pool(type, count);
}

void Context::destroy_query_pool(VkQueryPool pool) {
	context_impl->destroy_query_pool(pool);
}

// FRAME

size_t Context::max_frames_in_flight() const {
    // If context is headless, there is only one "frame" in flight
    if (is_headless()) return 1;
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

void Context::create_attachment(std::string_view name, VkExtent2D size, VkFormat format, ImageType type) {
	attachment_impl->create_attachment(name, size, format, type);
}

void Context::create_attachment(std::string_view name, VkExtent2D size, VkFormat format, VkSampleCountFlagBits samples, ImageType type) {
    attachment_impl->create_attachment(name, size, format, samples, type);
}

void Context::resize_attachment(std::string_view name, VkExtent2D new_size) {
	attachment_impl->resize_attachment(name, new_size);
}

bool Context::is_swapchain_attachment(std::string const& name) {
	return attachment_impl->is_swapchain_attachment(name);
}

bool Context::is_attachment(ImageView view) {
	return attachment_impl->is_attachment(view);
}

std::string Context::get_attachment_name(ImageView view) {
	return attachment_impl->get_attachment_name(view);
}

std::string Context::get_swapchain_attachment_name() const {
	return attachment_impl->get_swapchain_attachment_name();
}

// PIPELINE

ShaderHandle Context::create_shader(std::string_view path, std::string_view entry_point, ShaderStage stage) {
	return pipeline_impl->create_shader(path, entry_point, stage);
}

void Context::reflect_shaders(ph::PipelineCreateInfo& pci) {
	pipeline_impl->reflect_shaders(pci);
}

void Context::reflect_shaders(ph::ComputePipelineCreateInfo& pci) {
	pipeline_impl->reflect_shaders(pci);
}


void Context::create_named_pipeline(ph::PipelineCreateInfo pci) {
	pipeline_impl->create_named_pipeline(std::move(pci));
}

void Context::create_named_pipeline(ph::ComputePipelineCreateInfo pci) {
	pipeline_impl->create_named_pipeline(std::move(pci));
}

ShaderMeta const& Context::get_shader_meta(ph::Pipeline const& pipeline) {
	if (pipeline.type == ph::PipelineType::Graphics) return get_shader_meta(pipeline.name);
	else if (pipeline.type == ph::PipelineType::Compute) return get_compute_shader_meta(pipeline.name);
#if PHOBOS_ENABLE_RAY_TRACING
	else if (pipeline.type == ph::PipelineType::RayTracing) return get_ray_tracing_shader_meta(pipeline.name);
#endif
	assert(false && "Invalid pipeline type");
	PLIB_UNREACHABLE();
}

ShaderMeta const& Context::get_shader_meta(std::string_view pipeline_name) {
	return pipeline_impl->get_shader_meta(pipeline_name);
}

ShaderMeta const& Context::get_compute_shader_meta(std::string_view pipeline_name) {
	return pipeline_impl->get_compute_shader_meta(pipeline_name);
}

VkSampler Context::basic_sampler() {
	return pipeline_impl->basic_sampler;
}

#if PHOBOS_ENABLE_RAY_TRACING

void Context::create_named_pipeline(ph::RayTracingPipelineCreateInfo pci) {
	pipeline_impl->create_named_pipeline(std::move(pci));
}

void Context::reflect_shaders(ph::RayTracingPipelineCreateInfo& pci) {
	pipeline_impl->reflect_shaders(pci);
}

ShaderMeta const& Context::get_ray_tracing_shader_meta(std::string_view pipeline_name) {
	return pipeline_impl->get_ray_tracing_shader_meta(pipeline_name);
}

ShaderBindingTable Context::create_shader_binding_table(std::string_view pipeline_name) {
	return pipeline_impl->create_shader_binding_table(pipeline_name);
}

void Context::destroy_shader_binding_table(ShaderBindingTable& sbt) {
	destroy_buffer(sbt.buffer);
	sbt = {};
}

#endif

// IMAGE 

RawImage Context::create_image(ImageType type, VkExtent2D size, VkFormat format, uint32_t mips) {
	return image_impl->create_image(type, size, format, mips);
}

RawImage Context::create_image(ImageType type, VkExtent2D size, VkFormat format, VkSampleCountFlagBits samples, uint32_t mips) {
    return image_impl->create_image(type, size, format, samples, mips);
}

void Context::destroy_image(RawImage& image) {
	return image_impl->destroy_image(image);
}

ImageView Context::create_image_view(RawImage const& target, ImageAspect aspect) {
	return image_impl->create_image_view(target, aspect);
}

ImageView Context::create_image_view(RawImage const& target, uint32_t mip, ImageAspect aspect) {
    return image_impl->create_image_view(target, mip, aspect);
}

void Context::destroy_image_view(ImageView& view) {
	return image_impl->destroy_image_view(view);
}

ImageView Context::get_image_view(uint64_t id) {
	return image_impl->get_image_view(id);
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

VkDeviceAddress Context::get_device_address(RawBuffer const& buf) {
	return buffer_impl->get_device_address(buf);
}

VkDeviceAddress Context::get_device_address(BufferSlice slice) {
	return buffer_impl->get_device_address(slice);
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

Pipeline Context::get_or_create_pipeline(std::string_view name, VkRenderPass render_pass) {
	return cache_impl->get_or_create_pipeline(pipeline_impl->get_pipeline(name), render_pass);
}

Pipeline Context::get_or_create_compute_pipeline(std::string_view name) {
	return cache_impl->get_or_create_compute_pipeline(pipeline_impl->get_compute_pipeline(name));
}

#if PHOBOS_ENABLE_RAY_TRACING

Pipeline Context::get_or_create_ray_tracing_pipeline(std::string_view name) {
	return cache_impl->get_or_create_ray_tracing_pipeline(pipeline_impl->get_ray_tracing_pipeline(name));
}

#endif

VkDescriptorSet Context::get_or_create(DescriptorSetBinding const& set_binding, Pipeline const& pipeline, void* pNext) {
	return cache_impl->get_or_create_descriptor_set(set_binding, pipeline, pNext);
}

#if PHOBOS_ENABLE_RAY_TRACING

// RTX

void Context::destroy_acceleration_structure(AccelerationStructure& as) {
	for (auto& blas : as.bottom_level) {
		rtx_fun._vkDestroyAccelerationStructureKHR(device(), blas.handle, nullptr);
		destroy_buffer(blas.buffer);
	}
	as.bottom_level.clear();

	destroy_buffer(as.instance_buffer);
	rtx_fun._vkDestroyAccelerationStructureKHR(device(), as.top_level.handle, nullptr);
	destroy_buffer(as.top_level.buffer);
}

#endif

VkDevice Context::device() {
	return context_impl->device;
}

}
// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Vulkan initialization
//		- Context initialization
//		- Context destruction

#include <phobos/impl/context.hpp>
#include <phobos/impl/image.hpp>

#include <cassert>
#include <algorithm>

namespace ph {

InThreadContext::InThreadContext(ph::ScratchAllocator& vbo, ph::ScratchAllocator& ibo, ph::ScratchAllocator& ubo, ph::ScratchAllocator& ssbo)
	: vbo_allocator(vbo), ibo_allocator(ibo), ubo_allocator(ubo), ssbo_allocator(ssbo) {

}

BufferSlice InThreadContext::allocate_scratch_vbo(VkDeviceSize size) {
	return vbo_allocator.allocate(size);
}

BufferSlice InThreadContext::allocate_scratch_ibo(VkDeviceSize size) {
	return ibo_allocator.allocate(size);
}

BufferSlice InThreadContext::allocate_scratch_ubo(VkDeviceSize size) {
	return ubo_allocator.allocate(size);
}

BufferSlice InThreadContext::allocate_scratch_ssbo(VkDeviceSize size) {
	return ssbo_allocator.allocate(size);
}

namespace impl {

static std::optional<uint32_t> get_queue_family_prefer_dedicated(std::vector<VkQueueFamilyProperties> const& properties, VkQueueFlagBits required, VkQueueFlags avoid) {

	std::optional<uint32_t> best_match = std::nullopt;
	for (uint32_t i = 0; i < properties.size(); ++i) {
		VkQueueFlags flags = properties[i].queueFlags;
		if (!(flags & required)) { continue; }
		if (!(flags & avoid)) { return i; }

		best_match = i;
	}
	return best_match;
}

static PhysicalDevice select_physical_device(VkInstance instance, std::optional<SurfaceInfo> surface, GPURequirements const& requirements) {
	uint32_t device_count;
	VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	assert(result == VK_SUCCESS && device_count > 0 && "Could not find any vulkan-capable devices");
	std::vector<VkPhysicalDevice> devices(device_count);
	result = vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
	for (VkPhysicalDevice device : devices) {
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(device, &properties);
		VkPhysicalDeviceMemoryProperties mem_properties{};
		vkGetPhysicalDeviceMemoryProperties(device, &mem_properties);
#if PHOBOS_ENABLE_RAY_TRACING
		// Get ray tracing properties
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtx_properties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
		};
		VkPhysicalDeviceProperties2 properties_2{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &rtx_properties
		};
		vkGetPhysicalDeviceProperties2(device, &properties_2);
#endif
		// Check if the properties match the required settings
		if (requirements.dedicated && properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) continue;
		// Only do this check if there is a minimum video memory limit
		if (requirements.min_video_memory != 0) {
			uint64_t dedicated_video_memory = 0;
			for (uint32_t heap_idx = 0; heap_idx < mem_properties.memoryHeapCount; ++heap_idx) {
				VkMemoryHeap heap = mem_properties.memoryHeaps[heap_idx];
				if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
					dedicated_video_memory += heap.size;
				}
			}
			if (dedicated_video_memory < requirements.min_video_memory) continue;
		}

		// Check queues
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
		bool must_skip = false; // If we find a requested queue that isn't supported, skip this device.
		std::vector<QueueInfo> found_queues;
		for (QueueRequest queue : requirements.requested_queues) {
			// Find a queue with the desired capabilities
			VkQueueFlags avoid = {};
			if (queue.dedicated) {
				if (queue.type == QueueType::Graphics) avoid = VK_QUEUE_COMPUTE_BIT;
				if (queue.type == QueueType::Compute) avoid = VK_QUEUE_GRAPHICS_BIT;
				if (queue.type == QueueType::Transfer) avoid = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;
			}
			auto index = get_queue_family_prefer_dedicated(queue_families, static_cast<VkQueueFlagBits>(queue.type), avoid);
			// No queue was found
			if (index == std::nullopt) {
				must_skip = true;
				break;
			}

			// A queue was found.
			found_queues.push_back(QueueInfo{ .dedicated = !(queue_families[*index].queueFlags & avoid), .type = queue.type, .family_index = *index });
		}
		if (must_skip) continue;

		if (surface) {
			// Check support for our surface. We'll simply check every queue we are requesting for present support
			bool found_one = false;
			for (QueueInfo& queue : found_queues) {
				VkBool32 supported = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, queue.family_index, surface->handle, &supported);
				if (supported) {
					found_one = true;
					queue.can_present = true;
				}
			}
			if (!found_one) continue;
		}

		return {
			.handle = device,
			.properties = properties,
			.memory_properties = mem_properties,
			.found_queues = std::move(found_queues),
			.surface = surface,
#if PHOBOS_ENABLE_RAY_TRACING
			.ray_tracing_properties = rtx_properties
#endif
		};
	}

	// No matching device was found.
	return {};
}

static void fill_surface_details(PhysicalDevice& device) {
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.handle, device.surface->handle, &device.surface->capabilities);
	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device.handle, device.surface->handle, &format_count, nullptr);
	device.surface->formats.resize(format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device.handle, device.surface->handle, &format_count, device.surface->formats.data());
	uint32_t mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device.handle, device.surface->handle, &mode_count, nullptr);
	device.surface->present_modes.resize(mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device.handle, device.surface->handle, &mode_count, device.surface->present_modes.data());
}

static VkSurfaceFormatKHR choose_surface_format(AppSettings const& settings, PhysicalDevice const& device) {
	for (auto const& fmt : device.surface->formats) {
		// Srgb framebuffer so we can have gamma correction
		if (fmt.format == settings.surface_format.format && fmt.colorSpace == settings.surface_format.colorSpace) {
			return fmt;
		}
	}
	// If our preferred format isn't found, check if the fallback is supported. 
	VkSurfaceFormatKHR fallback = { .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	for (auto const& fmt : device.surface->formats) {
		if (fmt.format == fallback.format && fmt.colorSpace == fallback.colorSpace) {
			return fmt;
		}
	}
	// If it isn't, return the first format in the list
	return device.surface->formats[0];
}

static VkExtent2D choose_swapchain_extent(WindowInterface* wsi, PhysicalDevice const& device) {
	if (device.surface->capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return device.surface->capabilities.currentExtent;
	}
	else {
		// Clamp the extent to be within the allowed range
		VkExtent2D extent = VkExtent2D{ .width = wsi->width(), .height = wsi->height() };
		extent.width = std::clamp(extent.width, device.surface->capabilities.minImageExtent.width,
			device.surface->capabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, device.surface->capabilities.minImageExtent.height,
			device.surface->capabilities.maxImageExtent.height);
		return extent;
	}
}

static VkPresentModeKHR choose_present_mode(AppSettings const& settings, PhysicalDevice const& device) {
	for (auto const& mode : device.surface->present_modes) {
		if (mode == settings.present_mode) {
			return mode;
		}
	}
	// Vsync. The only present mode that is required to be supported
	return VK_PRESENT_MODE_FIFO_KHR;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
	VkDebugUtilsMessengerCallbackDataEXT const* callback_data, void* arg) {

	// Only log messages with severity 'warning' or above
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		ph::LogInterface* logger = reinterpret_cast<ph::LogInterface*>(arg);
		if (logger) {
			logger->write_fmt(ph::LogSeverity::Warning, "{}", callback_data->pMessage);
		}
	}

	return VK_FALSE;
}

ContextImpl::ContextImpl(AppSettings settings)
	: max_unbounded_array_size(settings.max_unbounded_array_size),
	max_frames_in_flight(settings.max_frames_in_flight),
	num_threads(settings.num_threads),
	has_validation(settings.enable_validation) {
	if (!settings.create_headless) {
		wsi = settings.wsi;
	}
	logger = settings.logger;

#if PHOBOS_ENABLE_RAY_TRACING
	// If ray tracing is enabled, add the required extensions for it
	settings.gpu_requirements.device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	settings.gpu_requirements.device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	settings.gpu_requirements.device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	settings.gpu_requirements.features_1_2.bufferDeviceAddress = true;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtx_features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
		.pNext = nullptr,
		.rayTracingPipeline = true
	};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_structure_features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
		.pNext = nullptr,
		.accelerationStructure = true
	};
	rtx_features.pNext = &accel_structure_features;
#endif

	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.apiVersion = VK_API_VERSION_1_2;
		app_info.pEngineName = "Phobos";
		app_info.engineVersion = VK_MAKE_VERSION(VERSION.major, VERSION.minor, VERSION.patch);
		app_info.pApplicationName = settings.app_name.data();
		app_info.applicationVersion = VK_MAKE_VERSION(settings.app_version.major, settings.app_version.minor, settings.app_version.patch);

		VkInstanceCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		info.pApplicationInfo = &app_info;

		if (validation_enabled()) {
			const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
			info.ppEnabledLayerNames = layers;
			info.enabledLayerCount = sizeof(layers) / sizeof(const char*);
		}

		std::vector<const char*> extensions;
		if (!is_headless()) {
			extensions = wsi->window_extension_names();
			if (validation_enabled()) {
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}
			info.ppEnabledExtensionNames = extensions.data();
			info.enabledExtensionCount = extensions.size();
		}

		VkResult result = vkCreateInstance(&info, nullptr, &instance);
		assert(result == VK_SUCCESS && "Failed to create VkInstance");
	}

	// Create debug messenger
	if (validation_enabled()) {
		auto create_func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		assert(create_func && "VK_EXT_debug_utils not present");

		// Create debug messenger
		VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
		messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		messenger_info.pUserData = logger;
		messenger_info.pfnUserCallback = vk_debug_callback;
		VkResult result = create_func(instance, &messenger_info, nullptr, &debug_messenger);
		assert(result == VK_SUCCESS && "Failed to create debug messenger");
	}

	if (!is_headless()) {
		phys_device.surface = SurfaceInfo{};
		phys_device.surface->handle = wsi->create_surface(instance);
	}

	phys_device = select_physical_device(instance, phys_device.surface, settings.gpu_requirements);
	if (!phys_device.handle) {
		logger->write(LogSeverity::Fatal, "Could not find a suitable physical device");
		return;
	}

	if (!is_headless()) {
		fill_surface_details(phys_device);
	}

	// Get the logical device using the queues we found.
	{
		std::vector<VkDeviceQueueCreateInfo> queue_infos;
		float const priority = 1.0f;
		for (QueueInfo queue : phys_device.found_queues) {
			// We need to make sure we request one set of queues for each family.
			if (std::find(family_indices.begin(), family_indices.end(), queue.family_index) == family_indices.end()) {
				VkDeviceQueueCreateInfo info{};
				info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				info.queueCount = std::count_if(phys_device.found_queues.begin(), phys_device.found_queues.end(), [&queue](QueueInfo const& q) {
					return q.family_index == queue.family_index;
				});
				info.queueFamilyIndex = queue.family_index;
				info.pQueuePriorities = &priority;
				queue_infos.push_back(info);

				family_indices.push_back(queue.family_index);
			}
		}

		VkDeviceCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		info.pQueueCreateInfos = queue_infos.data();
		info.queueCreateInfoCount = queue_infos.size();

		if (!is_headless()) {
			settings.gpu_requirements.device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}
		info.enabledExtensionCount = settings.gpu_requirements.device_extensions.size();
		info.ppEnabledExtensionNames = settings.gpu_requirements.device_extensions.data();

		// Build pNext chain for physical device features.
		settings.gpu_requirements._features.features = settings.gpu_requirements.features;
		settings.gpu_requirements._features.pNext = &settings.gpu_requirements.features_1_1;
		settings.gpu_requirements.features_1_1.pNext = &settings.gpu_requirements.features_1_2;
#if PHOBOS_ENABLE_RAY_TRACING
		settings.gpu_requirements.features_1_2.pNext = &rtx_features;
#endif

		// If we have a pNext chain both supplied by the user and in the features pNext chain we need to
		// chain them together.
		// To do this we'll iterate over the features pNext chain until we arrive at the end.
		// Then, we'll add the user pNext value at the end.
		// This way, this function also works when the user pNext chain is nullptr.
		if (settings.gpu_requirements._features.pNext) {
			// VkBaseOutStructure contains the sType and pNext of an item in the pNext chain.
			auto* user_pNext = reinterpret_cast<VkBaseOutStructure*>(settings.gpu_requirements.pNext);
			// Iteration start. We'll also set the final pNext to this pointer since it is the start of the
			// final pNext chain.
			auto* cur = reinterpret_cast<VkBaseOutStructure*>(settings.gpu_requirements._features.pNext);
			info.pNext = cur;
			// While there is a next item in the pNext chain
			while (cur->pNext != nullptr) {
				cur = cur->pNext;
			}
			// We've arrived at the end of the chain, we now append the user chain
			cur->pNext = user_pNext;
		}
		else {
			info.pNext = settings.gpu_requirements.pNext;
		}
		info.pEnabledFeatures = &settings.gpu_requirements.features;
		

		VkResult result = vkCreateDevice(phys_device.handle, &info, nullptr, &device);
		assert(result == VK_SUCCESS && "Failed to create logical device");
	}


	// Create VMA allocator
	{
		VmaAllocatorCreateInfo info{};
		info.device = device;
		info.instance = instance;
		info.physicalDevice = phys_device.handle;
		info.vulkanApiVersion = VK_API_VERSION_1_2;
		info.flags = VmaAllocatorCreateFlagBits::VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		vmaCreateAllocator(&info, &allocator);
	}

	// Grab debug utils naming function
	set_debug_utils_name_fun = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
}

void ContextImpl::post_init(Context& ctx, ImageImpl& image_impl, AppSettings const& settings) {
	img = &image_impl;

	// Get device queues
	{
		std::unordered_map<uint32_t /* Queue Family Index*/, uint32_t /*Queues already requested with this index*/> queue_counts;
		for (QueueInfo queue : phys_device.found_queues) {
			uint32_t index = queue_counts[queue.family_index];
			VkQueue handle = nullptr;
			vkGetDeviceQueue(device, queue.family_index, index, &handle);
			queues.emplace_back(ctx, queue, handle);
			queue_counts[queue.family_index] += 1;
			name_object(handle, fmt::format("[Queue] {} ({})", to_string(queue.type), index));
		}
	}

	// Create swapchain, but only if the context is not headless
	if (!is_headless()) {
		swapchain = Swapchain{};
		swapchain->format = choose_surface_format(settings, phys_device);
		swapchain->present_mode = choose_present_mode(settings, phys_device);
		swapchain->extent = choose_swapchain_extent(wsi, phys_device);

		uint32_t image_count = std::max(settings.min_swapchain_image_count, phys_device.surface->capabilities.minImageCount + 1);
		// Make sure not to exceed maximum. A value of 0 means there is no maximum
		if (uint32_t max_image_count = phys_device.surface->capabilities.maxImageCount; max_image_count != 0) {
			image_count = std::min(max_image_count, image_count);
		}

		VkSwapchainCreateInfoKHR info{};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.oldSwapchain = nullptr;
		info.surface = phys_device.surface->handle;
		info.imageFormat = swapchain->format.format;
		info.imageColorSpace = swapchain->format.colorSpace;
		info.imageExtent = swapchain->extent;
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		info.presentMode = swapchain->present_mode;
		info.minImageCount = image_count;
		info.clipped = true;
		info.preTransform = phys_device.surface->capabilities.currentTransform;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		VkResult result = vkCreateSwapchainKHR(device, &info, nullptr, &swapchain->handle);
		assert(result == VK_SUCCESS && "Failed to create swapchain");

		vkGetSwapchainImagesKHR(device, swapchain->handle, &image_count, nullptr);
		std::vector<VkImage> images(image_count);
		vkGetSwapchainImagesKHR(device, swapchain->handle, &image_count, images.data());

		swapchain->per_image = std::vector<Swapchain::PerImage>(image_count);
		for (size_t i = 0; i < swapchain->per_image.size(); ++i) {
			Swapchain::PerImage data{};
			data.image = {
				.type = ImageType::ColorAttachment,
				.format = swapchain->format.format,
				.size = swapchain->extent,
				.layers = 1,
				.mip_levels = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
				.handle = images[i],
				.memory = nullptr
			};

			data.view = img->create_image_view(data.image, ph::ImageAspect::Color);
			swapchain->per_image[i] = std::move(data);
		}
	}

	// Create thread contexts.
	if (num_threads > 0) {
		ptcs.reserve(num_threads);
		for (uint32_t i = 0; i < num_threads; ++i) {
			constexpr uint32_t vbo_alignment = 16;
			constexpr uint32_t ibo_alignment = 16;
			uint32_t const ubo_alignment = phys_device.properties.limits.minUniformBufferOffsetAlignment;
			uint32_t const ssbo_alignment = phys_device.properties.limits.minStorageBufferOffsetAlignment;
			ptcs.emplace_back(PerThreadContext{
				.vbo_allocator = ScratchAllocator(&ctx, settings.scratch_vbo_size, vbo_alignment, BufferType::VertexBufferDynamic),
				.ibo_allocator = ScratchAllocator(&ctx, settings.scratch_ibo_size, ibo_alignment, BufferType::IndexBufferDynamic),
				.ubo_allocator = ScratchAllocator(&ctx, settings.scratch_ubo_size, ubo_alignment, BufferType::MappedUniformBuffer),
				.ssbo_allocator = ScratchAllocator(&ctx, settings.scratch_ssbo_size, ssbo_alignment, BufferType::StorageBufferDynamic)
			});
			name_object(ptcs[i].vbo_allocator.get_buffer().handle, fmt::format("[Buffer] Thread - Scratch VBO ({})", i));
			name_object(ptcs[i].ibo_allocator.get_buffer().handle, fmt::format("[Buffer] Thread - Scratch IBO ({})", i));
			name_object(ptcs[i].ubo_allocator.get_buffer().handle, fmt::format("[Buffer] Thread - Scratch UBO ({})", i));
			name_object(ptcs[i].ssbo_allocator.get_buffer().handle, fmt::format("[Buffer] Thread - Scratch SSBO ({})", i));
		}
	}
}

ContextImpl::~ContextImpl() {
	// Destroy the queues. This will clean up the command pools they have in use
	queues.clear();

	if (validation_enabled()) {
		auto destroy_func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		destroy_func(instance, debug_messenger, nullptr);
	}
	if (!is_headless()) {
		for (Swapchain::PerImage& image : swapchain->per_image) {
			img->destroy_image_view(image.view);
			image.fence = nullptr;
			image.image = {};
		}

		vkDestroySwapchainKHR(device, swapchain->handle, nullptr);
		vkDestroySurfaceKHR(instance, phys_device.surface->handle, nullptr);
	}

	ptcs.clear();

	vmaDestroyAllocator(allocator);
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
}

bool ContextImpl::is_headless() const {
	return wsi == nullptr;
}

bool ContextImpl::validation_enabled() const {
	return has_validation;
}

uint32_t ContextImpl::thread_count() const {
	return num_threads;
}

Queue* ContextImpl::get_queue(QueueType type) {
	for (Queue& queue : queues) {
		if (queue.type() == type) {
			return &queue;
		}
	}
	return nullptr;
}

Queue* ContextImpl::get_present_queue() {
	for (Queue& queue : queues) {
		if (queue.can_present()) {
			return &queue;
		}
	}
	return nullptr;
}

void ContextImpl::next_frame() {
	for (Queue& queue : queues) {
		queue.next_frame();
	}
}

[[nodiscard]] InThreadContext ContextImpl::begin_thread(uint32_t thread_index) {
	log(LogSeverity::Debug, "Starting thread context #{}.", thread_index);

	PerThreadContext& ptc = ptcs[thread_index];
	return InThreadContext(ptc.vbo_allocator, ptc.ibo_allocator, ptc.ubo_allocator, ptc.ssbo_allocator);
}

void ContextImpl::end_thread(uint32_t thread_index) {
	log(LogSeverity::Debug, "Ending thread context #{}.", thread_index);
	ptcs[thread_index].vbo_allocator.reset();
	ptcs[thread_index].ibo_allocator.reset();
	ptcs[thread_index].ubo_allocator.reset();
	ptcs[thread_index].ssbo_allocator.reset();
}

template<typename VkT>
static void name_object_impl(VkT const handle, VkObjectType type, std::string const& name, VkDevice device, PFN_vkSetDebugUtilsObjectNameEXT fun) {
	VkDebugUtilsObjectNameInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	info.objectType = type;
	info.objectHandle = reinterpret_cast<uint64_t>(handle);
	info.pObjectName = name.c_str();
	fun(device, &info);
}

void ContextImpl::name_object(ph::Pipeline const& pipeline, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(pipeline.handle, VK_OBJECT_TYPE_PIPELINE, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(VkRenderPass pass, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(pass, VK_OBJECT_TYPE_RENDER_PASS, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(VkFramebuffer framebuf, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(framebuf, VK_OBJECT_TYPE_FRAMEBUFFER, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(VkBuffer buffer, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(buffer, VK_OBJECT_TYPE_BUFFER, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(VkImage image, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(image, VK_OBJECT_TYPE_IMAGE, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(ph::RawImage const& image, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(image.handle, VK_OBJECT_TYPE_IMAGE, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(ph::ImageView const& view, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(view.handle, VK_OBJECT_TYPE_IMAGE_VIEW, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(VkFence fence, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(fence, VK_OBJECT_TYPE_FENCE, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(VkSemaphore semaphore, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(semaphore, VK_OBJECT_TYPE_SEMAPHORE, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(VkQueue queue, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(queue, VK_OBJECT_TYPE_QUEUE, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(VkCommandPool pool, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(pool, VK_OBJECT_TYPE_COMMAND_POOL, name, device, set_debug_utils_name_fun);
	}
}

void ContextImpl::name_object(ph::CommandBuffer const& cmd_buf, std::string const& name) {
	if (validation_enabled() && !name.empty()) {
		name_object_impl(cmd_buf.handle(), VK_OBJECT_TYPE_COMMAND_BUFFER, name, device, set_debug_utils_name_fun);
	}
}

VkFence ContextImpl::create_fence() {
	VkFenceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence;
	vkCreateFence(device, &info, nullptr, &fence);
	return fence;
}

VkResult ContextImpl::wait_for_fence(VkFence fence, uint64_t timeout) {
	return vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);
}

void ContextImpl::reset_fence(VkFence fence) {
	vkResetFences(device, 1, &fence);
}

void ContextImpl::destroy_fence(VkFence fence) {
	vkDestroyFence(device, fence, nullptr);
}

VkSemaphore ContextImpl::create_semaphore() {
	VkSemaphoreCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {}
	};
	VkSemaphore semaphore;
	vkCreateSemaphore(device, &info, nullptr, &semaphore);
	return semaphore;
}

VkSampler ContextImpl::create_sampler(VkSamplerCreateInfo info) {
	VkSampler sampler;
	vkCreateSampler(device, &info, nullptr, &sampler);
	return sampler;
}

VkQueryPool ContextImpl::create_query_pool(VkQueryType type, uint32_t count) {
	VkQueryPoolCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.queryType = type,
		.queryCount = count
	};
	VkQueryPool pool{};
	vkCreateQueryPool(device, &info, nullptr, &pool);
	return pool;
}

void ContextImpl::destroy_query_pool(VkQueryPool pool) {
	vkDestroyQueryPool(device, pool, nullptr);
}

void ContextImpl::destroy_sampler(VkSampler sampler) {
	vkDestroySampler(device, sampler, nullptr);
}

void ContextImpl::destroy_semaphore(VkSemaphore semaphore) {
	vkDestroySemaphore(device, semaphore, nullptr);
}

LogInterface* ContextImpl::get_logger() {
	return logger;
}

std::vector<uint32_t> const& ContextImpl::queue_family_indices() const {
	return family_indices;
}

} // namespace impl
} // namespace ph
#include <phobos/context.hpp>

#include <cassert>
#include <limits>
#include <algorithm>

namespace ph {

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
	VkDebugUtilsMessengerCallbackDataEXT const* callback_data, void* arg) {

	// Only log messages with severity 'warning' or above
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		ph::LogInterface* log = reinterpret_cast<ph::LogInterface*>(arg);
		if (log) {
			log->write_fmt(ph::LogSeverity::Warning, "{}", callback_data->pMessage);
		}
	}

	return VK_FALSE;
}

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

static PhysicalDevice select_physical_device(VkInstance instance, SurfaceInfo const& surface, GPURequirements const& requirements) {
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

		// Check support for our surface. We'll simply check every queue we are requesting for present support
		bool found_one = false;
		for (QueueInfo& queue : found_queues) {
			VkBool32 supported = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, queue.family_index, surface.handle, &supported);
			if (supported) {
				found_one = true;
				queue.can_present = true;
			}
		}
		if (!found_one) continue;

		return { .handle = device, .properties = properties, .memory_properties = mem_properties, .found_queues = std::move(found_queues) };
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

Context::Context(AppSettings settings) {
	num_threads = settings.num_threads;
	has_validation = settings.enable_validation;
	if (!settings.create_headless) {
		wsi = settings.wsi;
		in_flight_frames = settings.max_frames_in_flight;
	}
	log = settings.log;

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
		messenger_info.pUserData = log;
		messenger_info.pfnUserCallback = vk_debug_callback;
		VkResult result = create_func(instance, &messenger_info, nullptr, &debug_messenger);
		assert(result == VK_SUCCESS && "Failed to create debug messenger");
	}

	phys_device.surface = SurfaceInfo{};
	phys_device.surface->handle = wsi->create_surface(instance);

	phys_device = select_physical_device(instance, *phys_device.surface, settings.gpu_requirements);
	if (!phys_device.handle) {
		log->write(LogSeverity::Fatal, "Could not find a suitable physical device");
		return;
	}

	fill_surface_details(phys_device);

	// Get the logical device using the queues we found.
	{
		std::vector<VkDeviceQueueCreateInfo> queue_infos;
		float const priority = 1.0f;
		for (QueueInfo queue : phys_device.found_queues) {
			VkDeviceQueueCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			info.queueCount = 1;
			info.queueFamilyIndex = queue.family_index;
			info.pQueuePriorities = &priority;
			queue_infos.push_back(info);
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
		info.pNext = settings.gpu_requirements.pNext;
		info.pEnabledFeatures = &settings.gpu_requirements.features;
		
		VkResult result = vkCreateDevice(phys_device.handle, &info, nullptr, &device);
		assert(result == VK_SUCCESS && "Failed to create logical device");
 	}

	// Get device queues
	{
		std::unordered_map<uint32_t /* Queue Family Index*/, uint32_t /*Queues already requested with this index*/> queue_counts;
		for (QueueInfo queue : phys_device.found_queues) {
			uint32_t index = queue_counts[queue.family_index];
			VkQueue handle = nullptr;
			vkGetDeviceQueue(device, queue.family_index, index, &handle);
			queues.emplace_back(*this, queue, handle);
			queue_counts[queue.family_index] += 1;
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
			data.view = ph::create_image_view(*this, data.image, ph::ImageAspect::Color);
			swapchain->per_image[i] = std::move(data);
		}

		per_frame = RingBuffer<PerFrame>(max_frames_in_flight());
		for (size_t i = 0; i < max_frames_in_flight(); ++i) {
			VkFence fence = nullptr;
			VkFenceCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			vkCreateFence(device, &info, nullptr, &fence);

			VkSemaphore semaphore = nullptr;
			VkSemaphore semaphore2 = nullptr;
			VkSemaphoreCreateInfo sem_info{};
			sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			vkCreateSemaphore(device, &sem_info, nullptr, &semaphore);
			vkCreateSemaphore(device, &sem_info, nullptr, &semaphore2);

			per_frame.set(i, PerFrame{ .fence = fence, .gpu_finished = semaphore, .image_ready = semaphore2 });
		}
	}
}

Context::~Context() {
	// First wait until there is no more running work.
	vkDeviceWaitIdle(device);

	// Destroy the queues. This will clean up the command pools they have in use
	queues.clear();

	if (validation_enabled()) {
		auto destroy_func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		destroy_func(instance, debug_messenger, nullptr);
	}
	if (!is_headless()) {
		for (PerFrame& frame : per_frame) {
			vkDestroyFence(device, frame.fence, nullptr);
			vkDestroySemaphore(device, frame.gpu_finished, nullptr);
			vkDestroySemaphore(device, frame.image_ready, nullptr);
		}

		for (Swapchain::PerImage& image : swapchain->per_image) {
			destroy_image_view(*this, image.view);
			image.fence = nullptr;
			image.image = {};
		}

		vkDestroySwapchainKHR(device, swapchain->handle, nullptr);
		vkDestroySurfaceKHR(instance, phys_device.surface->handle, nullptr);
	}
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
}

bool Context::is_headless() const {
	return wsi == nullptr;
}

bool Context::validation_enabled() const {
	return has_validation;
}

uint32_t Context::thread_count() const {
	return num_threads;
}

Queue* Context::get_queue(QueueType type) {
	for (Queue& queue : queues) {
		if (queue.type() == type) {
			return &queue;
		}
	}
	return nullptr;
}

Queue* Context::get_present_queue() {
	for (Queue& queue : queues) {
		if (queue.can_present()) {
			return &queue;
		}
	}
	return nullptr;
}

size_t Context::max_frames_in_flight() const {
	return in_flight_frames;
}

void Context::wait_for_frame() {
	// Wait for an available frame context
	PerFrame& frame_data = per_frame.current();
	vkWaitForFences(device, 1, &frame_data.fence, true, std::numeric_limits<uint64_t>::max());

	// Get an image index to present to
	vkAcquireNextImageKHR(device, swapchain->handle, std::numeric_limits<uint64_t>::max(), frame_data.image_ready, nullptr, &swapchain->image_index);

	// Wait until the image is absolutely no longer in use. This can happen when there are more frames in flight than swapchain images, or when
	// vkAcquireNextImageKHR returns indices out of order
	if (swapchain->per_image[swapchain->image_index].fence != nullptr) {
		vkWaitForFences(device, 1, &swapchain->per_image[swapchain->image_index].fence, true, std::numeric_limits<uint64_t>::max());
	}
	// Mark this image as in use by the current frame
	swapchain->per_image[swapchain->image_index].fence = frame_data.fence;

	// Once we have a frame we need to update where the swapchain attachment in our attachments list is pointing to
	attachments[std::string(swapchain_attachment_name)] = Attachment{ .view = swapchain->per_image[swapchain->image_index].view };
}

void Context::submit_frame_commands(Queue& queue, CommandBuffer& cmd_buf) {
	PerFrame& frame_data = per_frame.current();

	// Reset our fence from last time so we can use it again now
	vkResetFences(device, 1, &frame_data.fence);

	queue.submit(cmd_buf, frame_data.fence, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, frame_data.image_ready, frame_data.gpu_finished);
}

void Context::present(Queue& queue) {
	assert(!is_headless() && "Tried presenting from a headless context.\n");

	PerFrame& frame_data = per_frame.current();
	
	// Present to chosen queue
	VkPresentInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.swapchainCount = 1;
	info.pSwapchains = &swapchain->handle;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &frame_data.gpu_finished;
	info.pImageIndices = &swapchain->image_index;
	queue.present(info);

	// Advance per-frame ringbuffers to the next element
	next_frame();
}

Attachment* Context::get_attachment(std::string_view name) {
	std::string key{ name };
	if (auto it = attachments.find(key); it != attachments.end()) {
		return &it->second;
	}
	return nullptr;
}

bool Context::is_swapchain_attachment(std::string const& name) {
	bool result = name == swapchain_attachment_name;
	return result;
}

std::string Context::get_swapchain_attachment_name() const {
	return swapchain_attachment_name;
}

void Context::next_frame() {
	per_frame.next();
	for (Queue& queue : queues) {
		queue.next_frame();
	}
}

}
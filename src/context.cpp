#include <phobos/context.hpp>

#include <cassert>
#include <limits>
#include <algorithm>
#include <spirv_cross.hpp>

namespace ph {

namespace reflect {
	static PipelineStage get_shader_stage(spirv_cross::Compiler& refl) {
		auto entry_point_name = refl.get_entry_points_and_stages()[0];
		auto entry_point = refl.get_entry_point(entry_point_name.name, entry_point_name.execution_model);

		switch (entry_point.model) {
		case spv::ExecutionModel::ExecutionModelVertex: return PipelineStage::VertexShader;
		case spv::ExecutionModel::ExecutionModelFragment: return PipelineStage::FragmentShader;
		case spv::ExecutionModel::ExecutionModelGLCompute: return PipelineStage::ComputeShader;
		default: return {};
		}
	}

	static VkFormat get_vk_format(uint32_t vecsize) {
		switch (vecsize) {
		case 1: return VK_FORMAT_R32_SFLOAT;
		case 2: return VK_FORMAT_R32G32_SFLOAT;
		case 3: return VK_FORMAT_R32G32B32_SFLOAT;
		case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		default: return VK_FORMAT_UNDEFINED;
		}
	}

	static std::unique_ptr<spirv_cross::Compiler> reflect_shader_stage(ShaderModuleCreateInfo& shader) {
		auto refl = std::make_unique<spirv_cross::Compiler>(shader.code);

		assert(refl && "Failed to reflect shader");
		spirv_cross::ShaderResources res = refl->get_shader_resources();

		return refl;
	}

	static VkShaderStageFlags pipeline_to_shader_stage(PipelineStage stage) {
		switch (stage) {
		case PipelineStage::VertexShader: return VK_SHADER_STAGE_VERTEX_BIT;
		case PipelineStage::FragmentShader: return VK_SHADER_STAGE_FRAGMENT_BIT;
		case PipelineStage::ComputeShader: return VK_SHADER_STAGE_COMPUTE_BIT;
		default: return {};
		}
	}

	// This function merges all push constant ranges in a single shader stage into one push constant range.
	// We need this because spirv-cross reports one push constant range for each variable, instead of for each block.
	static void merge_ranges(std::vector<VkPushConstantRange>& out, std::vector<VkPushConstantRange> const& in,
		PipelineStage stage) {

		VkPushConstantRange merged{};
		merged.size = 0;
		merged.offset = 1000000; // some arbitrarily large value to start with, we'll make this smaller using std::min() later
		merged.stageFlags = pipeline_to_shader_stage(stage);
		for (auto& range : in) {
			if (range.stageFlags == merged.stageFlags) {
				merged.offset = std::min(merged.offset, range.offset);
				merged.size += range.size;
			}
		}

		if (merged.size != 0) {
			out.push_back(merged);
		}
	}

	static std::vector<VkPushConstantRange> get_push_constants(std::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders) {
		std::vector<VkPushConstantRange> pc_ranges;
		std::vector<VkPushConstantRange> final;
		for (auto& refl : reflected_shaders) {
			spirv_cross::ShaderResources res = refl->get_shader_resources();
			auto const stage = get_shader_stage(*refl);
			for (auto& pc : res.push_constant_buffers) {
				auto ranges = refl->get_active_buffer_ranges(pc.id);
				for (auto& range : ranges) {
					VkPushConstantRange pc_range{};
					pc_range.offset = range.offset;
					pc_range.size = range.range;
					pc_range.stageFlags = pipeline_to_shader_stage(stage);
					pc_ranges.push_back(pc_range);
				}
			}
			merge_ranges(final, pc_ranges, stage);
		}
		return final;
	}

	static void find_uniform_buffers(ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = pipeline_to_shader_stage(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();
		for (auto& ubo : res.uniform_buffers) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(ubo.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.descriptorCount = 1;
			binding.stageFlags = stage;
			dslci.bindings.push_back(binding);

			info.add_binding(refl.get_name(ubo.id), { binding.binding, binding.descriptorType });
		}
	}

	static void find_shader_storage_buffers(ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = pipeline_to_shader_stage(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();
		for (auto& ssbo : res.storage_buffers) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(ssbo.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.descriptorCount = 1;
			binding.stageFlags = stage;
			dslci.bindings.push_back(binding);

			info.add_binding(refl.get_name(ssbo.id), { binding.binding, binding.descriptorType });
		}
	}

	static void find_sampled_images(Context& ctx, ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = pipeline_to_shader_stage(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();
		for (auto& si : res.sampled_images) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(si.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			binding.stageFlags = stage;
			auto type = refl.get_type(si.type_id);
			// type.array has the dimensions of the array. If this is zero, we don't have an array.
			// If it's larger than zero, we have an array.
			if (type.array.size() > 0) {
				// Now the dimensions of the array are in the first value of the array field.
				// 0 means unbounded
				if (type.array[0] == 0) {
					binding.descriptorCount = ctx.max_unbounded_array_size;
					// An unbounded array of samplers means descriptor indexing, we have to set the PartiallyBound and VariableDescriptorCount
					// flags for this binding

					// Reserve enough space to hold all flags and this one
					dslci.flags.resize(dslci.bindings.size() + 1);
					dslci.flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
				}
				else {
					binding.descriptorCount = type.array[0];
				}
			}
			else {
				// If it' not an array, there is only one descriptor
				binding.descriptorCount = 1;
			}

			info.add_binding(refl.get_name(si.id), { binding.binding, binding.descriptorType });
			dslci.bindings.push_back(binding);
		}
	}

	static void find_storage_images(ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = pipeline_to_shader_stage(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();

		for (auto& si : res.storage_images) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(si.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			binding.stageFlags = stage;
			binding.descriptorCount = 1;
			info.add_binding(refl.get_name(si.id), { binding.binding, binding.descriptorType });
			dslci.bindings.push_back(binding);
		}
	}

	static void collapse_bindings(DescriptorSetLayoutCreateInfo& info) {
		std::vector<VkDescriptorSetLayoutBinding> final_bindings;
		std::vector<VkDescriptorBindingFlags> final_flags;

		for (size_t i = 0; i < info.bindings.size(); ++i) {
			auto binding = info.bindings[i];

			// Before doing anything, check if we already processed this binding
			bool process = true;
			for (auto const& b : final_bindings) {
				if (binding.binding == b.binding) {
					process = false;
					break;
				}
			}
			if (!process) { continue; }

			VkShaderStageFlags stages = binding.stageFlags;
			for (size_t j = i + 1; j < info.bindings.size(); ++j) {
				auto const& other_binding = info.bindings[j];
				if (binding.binding == other_binding.binding) {
					stages |= other_binding.stageFlags;

				}
			}
			binding.stageFlags = stages;
			final_bindings.push_back(binding);
			if (!info.flags.empty()) {
				final_flags.push_back(info.flags[i]);
			}
		}

		info.bindings = final_bindings;
		info.flags = final_flags;
	}

	static DescriptorSetLayoutCreateInfo get_descriptor_set_layout(Context& ctx, ShaderMeta& info, std::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders) {
		DescriptorSetLayoutCreateInfo dslci;
		for (auto& refl : reflected_shaders) {
			find_uniform_buffers(info, *refl, dslci);
			find_shader_storage_buffers(info, *refl, dslci);
			find_sampled_images(ctx, info, *refl, dslci);
			find_storage_images(info, *refl, dslci);
		}
		collapse_bindings(dslci);
		return dslci;
	}

	static PipelineLayoutCreateInfo make_pipeline_layout(Context& ctx, std::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders, ShaderMeta& shader_info) {
		PipelineLayoutCreateInfo layout;
		layout.push_constants = get_push_constants(reflected_shaders);
		layout.set_layout = get_descriptor_set_layout(ctx, shader_info, reflected_shaders);
		return layout;
	}
}

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

		return { .handle = device, .properties = properties, .memory_properties = mem_properties, .found_queues = std::move(found_queues), .surface = surface };
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
	max_unbounded_array_size = settings.max_unbounded_array_size;

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

	if (!is_headless()) {
		phys_device.surface = SurfaceInfo{};
		phys_device.surface->handle = wsi->create_surface(instance);
	}

	phys_device = select_physical_device(instance, phys_device.surface, settings.gpu_requirements);
	if (!phys_device.handle) {
		log->write(LogSeverity::Fatal, "Could not find a suitable physical device");
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

	VkDescriptorPoolCreateInfo dpci{};
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.flags = {};
	dpci.pNext = nullptr;
	VkDescriptorPoolSize pool_sizes[]{
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets_per_type },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sets_per_type },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sets_per_type },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, sets_per_type }
	};
	dpci.maxSets = sets_per_type * sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize);
	dpci.poolSizeCount = sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize);
	dpci.pPoolSizes = pool_sizes;
	vkCreateDescriptorPool(device, &dpci, nullptr, &descr_pool);

	cache.descriptor_set = RingBuffer<Cache<DescriptorSetBinding, VkDescriptorSet>>{ settings.max_frames_in_flight };

	// Create basic sampler
	{
		VkSamplerCreateInfo sci{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = false,
			.maxAnisotropy = 0.0f,
			.compareEnable = false,
			.compareOp = VK_COMPARE_OP_ALWAYS,
			.minLod = 0.0f,
			.maxLod = 64.0f,
			.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
			.unnormalizedCoordinates = false
		};
		vkCreateSampler(device, &sci, nullptr, &basic_sampler);
	}

	// Create VMA allocator
	{
		VmaAllocatorCreateInfo info{};
		info.device = device;
		info.instance = instance;
		info.physicalDevice = phys_device.handle;
		info.vulkanApiVersion = VK_API_VERSION_1_2;
		vmaCreateAllocator(&info, &allocator);
	}
}

Context::~Context() {
	// First wait until there is no more running work.
	vkDeviceWaitIdle(device);

	// Destroy the queues. This will clean up the command pools they have in use
	queues.clear();

	// Clear out all caches
	cache.framebuffer.foreach([this](VkFramebuffer framebuf) {
		vkDestroyFramebuffer(device, framebuf, nullptr);
	});
	cache.renderpass.foreach([this](VkRenderPass pass) {
		vkDestroyRenderPass(device, pass, nullptr);
	});

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
	attachments[std::string(swapchain_attachment_name)] = 
		InternalAttachment{ 
			Attachment{ .view = swapchain->per_image[swapchain->image_index].view }, 
			std::nullopt
	};
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
		return &it->second.attachment;
	}
	return nullptr;
}

void Context::create_attachment(std::string_view name, VkExtent2D size, VkFormat format) {
	InternalAttachment attachment{};
	// Create image and image view
	attachment.image = create_image(*this, is_depth_format(format) ? ImageType::DepthStencilAttachment : ImageType::ColorAttachment, size, format);
	attachment.attachment.view = create_image_view(*this, *attachment.image, is_depth_format(format) ? ImageAspect::Depth : ImageAspect::Color);
	attachments[std::string{ name }] = attachment;
}

bool Context::is_swapchain_attachment(std::string const& name) {
	bool result = name == swapchain_attachment_name;
	return result;
}

std::string Context::get_swapchain_attachment_name() const {
	return swapchain_attachment_name;
}

VkFramebuffer Context::get_or_create_framebuffer(VkFramebufferCreateInfo const& info) {
	{
		VkFramebuffer* framebuf = cache.framebuffer.get(info);
		if (framebuf) { return *framebuf; }
	}

	VkFramebuffer framebuf = nullptr;
	vkCreateFramebuffer(device, &info, nullptr, &framebuf);
	cache.framebuffer.insert(info, framebuf);
	return framebuf;
}

VkRenderPass Context::get_or_create_renderpass(VkRenderPassCreateInfo const& info) {
	{
		VkRenderPass* pass = cache.renderpass.get(info);
		if (pass) { return *pass; }
	}

	VkRenderPass pass = nullptr;
	vkCreateRenderPass(device, &info, nullptr, &pass);
	cache.renderpass.insert(info, pass);
	return pass;
}

VkDescriptorSetLayout Context::get_or_create_descriptor_set_layout(DescriptorSetLayoutCreateInfo const& dslci) {
	auto set_layout_opt = cache.set_layout.get(dslci);
	if (!set_layout_opt) {
		// We have to create the descriptor set layout here
		VkDescriptorSetLayoutCreateInfo set_layout_info{ };
		set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set_layout_info.bindingCount = dslci.bindings.size();
		set_layout_info.pBindings = dslci.bindings.data();
		VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{};
		if (!dslci.flags.empty()) {
			flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			assert(dslci.bindings.size() == dslci.flags.size() && "flag count doesn't match binding count");
			flags_info.bindingCount = dslci.bindings.size();
			flags_info.pBindingFlags = dslci.flags.data();
			set_layout_info.pNext = &flags_info;
		}
		VkDescriptorSetLayout set_layout = nullptr;
		vkCreateDescriptorSetLayout(device, &set_layout_info, nullptr, &set_layout);
		// Store for further use when creating the pipeline layout
		cache.set_layout.insert(dslci, set_layout);
		return set_layout;
	}
	else {
		return *set_layout_opt;
	}
}

PipelineLayout Context::get_or_create_pipeline_layout(PipelineLayoutCreateInfo const& plci, VkDescriptorSetLayout set_layout) {
	// Create or get pipeline layout from cache
	auto pipeline_layout_opt = cache.pipeline_layout.get(plci);
	if (!pipeline_layout_opt) {
		// We have to create a new pipeline layout
		VkPipelineLayoutCreateInfo layout_create_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.setLayoutCount = 1,
			.pSetLayouts = &set_layout,
			.pushConstantRangeCount = (uint32_t)plci.push_constants.size(),
			.pPushConstantRanges = plci.push_constants.data(),
		};
		VkPipelineLayout vk_layout = nullptr;
		vkCreatePipelineLayout(device, &layout_create_info, nullptr, &vk_layout);
		PipelineLayout layout;
		layout.handle = vk_layout;
		layout.set_layout = set_layout;
		cache.pipeline_layout.insert(plci, layout);
		return layout;
	}
	else {
		return *pipeline_layout_opt;
	}
}

static VkShaderModule create_shader_module(Context& ctx, ShaderModuleCreateInfo const& info) {
	VkShaderModuleCreateInfo vk_info{};
	vk_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	constexpr size_t bytes_per_spv_element = 4;
	vk_info.codeSize = info.code.size() * bytes_per_spv_element;
	vk_info.pCode = info.code.data();
	VkShaderModule module = nullptr;
	vkCreateShaderModule(ctx.device, &vk_info, nullptr, &module);
	return module;
}

Pipeline Context::get_or_create_pipeline(std::string_view name, VkRenderPass render_pass) {
	ph::PipelineCreateInfo& pci = pipelines[std::string(name)];
	{
		Pipeline* pipeline = cache.pipeline.get(pci);
		if (pipeline) { return *pipeline; }
	}

	// Set up pipeline create info
	VkGraphicsPipelineCreateInfo gpci{};
	VkDescriptorSetLayout set_layout = get_or_create_descriptor_set_layout(pci.layout.set_layout);
	ph::PipelineLayout layout = get_or_create_pipeline_layout(pci.layout, set_layout);
	gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpci.layout = layout.handle;
	gpci.renderPass = render_pass;
	gpci.subpass = 0;
	VkPipelineColorBlendStateCreateInfo blend_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.logicOpEnable = pci.blend_logic_op_enable,
		.logicOp = {},
		.attachmentCount = (uint32_t)pci.blend_attachments.size(),
		.pAttachments = pci.blend_attachments.data(),
		.blendConstants = {}
	};
	gpci.pColorBlendState = &blend_info;
	gpci.pDepthStencilState = &pci.depth_stencil;
	VkPipelineDynamicStateCreateInfo dynamic_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.dynamicStateCount = (uint32_t)pci.dynamic_states.size(),
		.pDynamicStates = pci.dynamic_states.data()
	};
	gpci.pDynamicState = &dynamic_state;
	gpci.pInputAssemblyState = &pci.input_assembly;
	gpci.pMultisampleState = &pci.multisample;
	gpci.pNext = nullptr;
	gpci.pRasterizationState = &pci.rasterizer;
	VkPipelineVertexInputStateCreateInfo vertex_input{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.vertexBindingDescriptionCount = (uint32_t)pci.vertex_input_bindings.size(),
		.pVertexBindingDescriptions = pci.vertex_input_bindings.data(),
		.vertexAttributeDescriptionCount = (uint32_t)pci.vertex_attributes.size(),
		.pVertexAttributeDescriptions = pci.vertex_attributes.data()
	};
	gpci.pVertexInputState = &vertex_input;
	VkPipelineViewportStateCreateInfo viewport_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.viewportCount = (uint32_t)pci.viewports.size(),
		.pViewports = pci.viewports.data(),
		.scissorCount = (uint32_t)pci.scissors.size(),
		.pScissors = pci.scissors.data()
	};
	gpci.pViewportState = &viewport_state;
	
	// Shader modules
	std::vector<VkPipelineShaderStageCreateInfo> shader_infos;
	for (auto const& handle : pci.shaders) {
		auto shader_info = cache.shader.get(handle);
		assert(shader_info && "Invalid shader handle");
		VkShaderStageFlagBits stage{};
		if (shader_info->stage == PipelineStage::VertexShader) stage = VK_SHADER_STAGE_VERTEX_BIT;
		else if (shader_info->stage == PipelineStage::FragmentShader) stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		else if (shader_info->stage == PipelineStage::ComputeShader) stage = VK_SHADER_STAGE_COMPUTE_BIT;
		else assert(false && "Invalid shader stage");
		VkPipelineShaderStageCreateInfo ssci{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.stage = stage,
			.module = create_shader_module(*this, *shader_info),
			.pName = shader_info->entry_point.c_str(),
			.pSpecializationInfo = nullptr
		};
		shader_infos.push_back(ssci);
	}
	gpci.stageCount = shader_infos.size();
	gpci.pStages = shader_infos.data();

	Pipeline pipeline;
	vkCreateGraphicsPipelines(device, nullptr, 1, &gpci, nullptr, &pipeline.handle);
	pipeline.name = pci.name;
	pipeline.layout = layout;
	pipeline.type = PipelineType::Graphics;

	cache.pipeline.insert(pci, pipeline);

	return pipeline;
}

VkDescriptorSet Context::get_or_create_descriptor_set(DescriptorSetBinding set_binding, Pipeline const& pipeline, void* pNext) {
	set_binding.set_layout = pipeline.layout.set_layout;
	auto set_opt = cache.descriptor_set.current().get(set_binding);
	if (set_opt) {
		return *set_opt;
	}
	// Create descriptor set layout and issue writes
	VkDescriptorSetAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pSetLayouts = &set_binding.set_layout;
	// add default descriptor pool if no custom one was specified
	if (!set_binding.pool) { set_binding.pool = descr_pool; };
	alloc_info.descriptorPool = set_binding.pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pNext = pNext;

	VkDescriptorSet set = nullptr;
	vkAllocateDescriptorSets(device, &alloc_info, &set);

	// Now we have the set we need to write the requested data to it
	std::vector<VkWriteDescriptorSet> writes;
	struct DescriptorWriteInfo {
		std::vector<VkDescriptorBufferInfo> buffer_infos;
		std::vector<VkDescriptorImageInfo> image_infos;
	};
	std::vector<DescriptorWriteInfo> write_infos;
	writes.reserve(set_binding.bindings.size());
	for (auto const& binding : set_binding.bindings) {
		if (binding.descriptors.empty()) continue;
		DescriptorWriteInfo write_info;
		VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr
		};
		write.dstSet = set;
		write.dstBinding = binding.binding;
		write.descriptorType = binding.type;
		write.descriptorCount = binding.descriptors.size();
		switch (binding.type) {
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
				write_info.image_infos.reserve(binding.descriptors.size());
				for (auto const& descriptor : binding.descriptors) {
					VkDescriptorImageInfo img{
						.sampler = descriptor.image.sampler,
						.imageView = descriptor.image.view.handle,
						.imageLayout = descriptor.image.layout
					};
					write_info.image_infos.push_back(img);
					// Push dummy buffer info to make sure indices match
					write_info.buffer_infos.emplace_back();
				}
		} break;
		default: {
				auto& info = binding.descriptors.front().buffer;
				VkDescriptorBufferInfo buf{
					.buffer = info.buffer,
					.offset = info.offset,
					.range = info.range
				};

				write_info.buffer_infos.push_back(buf);
				// Push dummy image info to make sure indices match
				write_info.image_infos.emplace_back();
			} break;
		}
		write_infos.push_back(write_info);
		writes.push_back(write);
	}

	for (size_t i = 0; i < write_infos.size(); ++i) {
		switch (writes[i].descriptorType) {
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
				writes[i].pImageInfo = write_infos[i].image_infos.data();
			} break;
			default: {
				writes[i].pBufferInfo = write_infos[i].buffer_infos.data();
			} break;
		}
	}

	vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
	cache.descriptor_set.current().insert(set_binding, set);
	return set;
}

namespace {

struct ShaderHandleId {
	static inline std::atomic<uint32_t> cur = 0;
	static inline uint32_t next() {
		return cur++;
	}
};

}

ShaderHandle Context::create_shader(std::string_view path, std::string_view entry_point, PipelineStage stage) {
	uint32_t id = ShaderHandleId::next();

	ph::ShaderModuleCreateInfo info;
	info.code = load_shader_code(path);
	info.entry_point = entry_point;
	info.stage = stage;

	cache.shader.insert(ShaderHandle{ id }, std::move(info));

	return ShaderHandle{ id };
}

void Context::reflect_shaders(ph::PipelineCreateInfo& pci) {
	std::vector<std::unique_ptr<spirv_cross::Compiler>> reflected_shaders;
	for (auto handle : pci.shaders) {
		ph::ShaderModuleCreateInfo* shader = cache.shader.get(handle);
		assert(shader && "Invalid shader");
		reflected_shaders.push_back(reflect::reflect_shader_stage(*shader));
	}
	pci.layout = reflect::make_pipeline_layout(*this, reflected_shaders, pci.meta);
}

void Context::create_named_pipeline(ph::PipelineCreateInfo pci) {
	pipelines[pci.name] = std::move(pci);
}

ShaderMeta const& Context::get_shader_meta(std::string_view pipeline_name) {
	return pipelines.at(std::string(pipeline_name)).meta;
}

void Context::next_frame() {
	per_frame.next();
	for (Queue& queue : queues) {
		queue.next_frame();
	}
	cache.descriptor_set.next();
}

}
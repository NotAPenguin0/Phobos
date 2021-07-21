#include <phobos/context.hpp>
#include <phobos/render_graph.hpp>
#include <GLFW/glfw3.h>

#include <iostream>

class GLFWWindowInterface : public ph::WindowInterface {
public:
	GLFWWindowInterface(const char* title, uint32_t width, uint32_t height) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	}

	~GLFWWindowInterface() {
		glfwDestroyWindow(window);
	}

	std::vector<const char*> window_extension_names() const override {
		uint32_t count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&count);
		return std::vector<const char*>(extensions, extensions + count);
	}

	VkSurfaceKHR create_surface(VkInstance instance) const override {
		VkSurfaceKHR surface;
		glfwCreateWindowSurface(instance, window, nullptr, &surface);
		return surface;
	}

	GLFWwindow* handle() {
		return window;
	}

	void poll_events() {
		glfwPollEvents();
	}

	bool is_open() {
		return !glfwWindowShouldClose(window);
	}

	bool close() {
		glfwSetWindowShouldClose(window, true);
	}
	
	uint32_t width() const override {
		int w = 0;
		glfwGetFramebufferSize(window, &w, nullptr);
		return w;
	}

	uint32_t height() const override {
		int h = 0;
		glfwGetFramebufferSize(window, nullptr, &h);
		return h;
	}

private:
	GLFWwindow* window = nullptr;
};

static std::string_view sev_string(ph::LogSeverity sev) {
	switch (sev) {
	case ph::LogSeverity::Debug:
		return "[DEBUG]";
	case ph::LogSeverity::Info:
		return "[INFO]";
	case ph::LogSeverity::Warning:
		return "[WARNING]";
	case ph::LogSeverity::Error:
		return "[ERROR]";
	case ph::LogSeverity::Fatal:
		return "[FATAL]";
	}
}

class StdoutLogger : public ph::LogInterface {
public:
	void write(ph::LogSeverity sev, std::string_view message) {
		if (timestamp_enabled) {
			std::cout << get_timestamp_string() << " ";
		}
		std::cout << sev_string(sev) << ": " << message << std::endl;
	}
};

// We make this a struct to demonstrate typed buffers
struct vec4 {
	float x, y, z, w;
};

struct vec3 {
	float x, y, z;
};

vec4 vertices[] = { 
	vec4(-1.0, 1.0, 0.0, 1.0),
	vec4(-1.0, -1.0, 0.0, 0.0),
	vec4(1.0, -1.0, 1.0, 0.0),
	vec4(-1.0, 1.0, 0, 1.0),
	vec4(1.0, -1.0, 1.0, 0.0),
	vec4(1.0, 1.0, 1.0, 1.0)
};

struct ShaderSSBO {
	vec3 color;
};

// TEST APP CHECLIST FOR MT SUPPORT
// 1. Create simple task system
// 2. Create async task that writes to an image on the gpu
// 3. Create renderpass on main thread that does not do anything if image is not ready, if it is ready make it display the image
// 4. Log frames where the image isn't drawn to ensure async-ness.

struct Task {
	virtual void execute(uint32_t thread_index) = 0;
	virtual bool poll() = 0;
	virtual void cleanup(uint32_t thread_index) = 0;
};

class TaskScheduler {
public:
	TaskScheduler(int num_threads) {
		threads.resize(num_threads);
	}

	void try_run(Task* task) {
		for (uint32_t i = 0; i < threads.size(); ++i) {
			auto& thread = threads[i];
			if (!thread.busy) {
				thread.thread = std::thread(&Task::execute, task, i);
				thread.task = task;
				thread.busy = true;
				break;
			}
		}
	}

	void poll() {
		for (uint32_t i = 0; i < threads.size(); ++i) {
			auto& thread = threads[i];
			if (thread.task == nullptr) continue;
			if (thread.task->poll()) {
				thread.task->cleanup(i);
				thread.busy = false;
				thread.task = nullptr;
				thread.thread.join();
			}
		}
	}
	
	bool task_done(Task* task) {
		for (auto& thread : threads) {
			if (thread.task == task) return false;
		}
		return true;
	}

private:
	struct TaskThread {
		std::thread thread;
		Task* task = nullptr;
		bool busy = false;
	};

	std::vector<TaskThread> threads;
};

struct ImageWriteTask : public Task {
	ph::Context& ctx;
	ph::ImageView target_image;
	VkFence fence = nullptr;
	VkSemaphore semaphore = nullptr;
	ph::CommandBuffer cmd_buffer;

	ImageWriteTask(ph::Context& ctx, ph::ImageView image) : ctx(ctx), target_image(image) {
		fence = ctx.create_fence();
		semaphore = ctx.create_semaphore();
	}

	void execute(uint32_t thread_index) override {
		ph::InThreadContext itc = ctx.begin_thread(thread_index);
		ph::Pass image_write = ph::PassBuilder::create_compute("image_write_async")
			.write_storage_image(target_image, ph::PipelineStage::ComputeShader)
			.execute([this, &itc](ph::CommandBuffer& cmd_buf) {
				cmd_buf.bind_compute_pipeline("compute_write");
				VkDescriptorSet set = ph::DescriptorBuilder::create(ctx, cmd_buf.get_bound_pipeline())
					.add_storage_image("out_img", target_image)
					.get();
				cmd_buf.bind_descriptor_set(set);

				cmd_buf.dispatch(2048 / 32, 2048 / 32, 1);
			})
		.get();
		ph::RenderGraph graph;
		graph.add_pass(image_write);
		graph.build(ctx);
		ph::RenderGraphExecutor executor{};
		
		ph::Queue* compute = ctx.get_queue(ph::QueueType::Compute);
		ph::Queue* graphics = ctx.get_queue(ph::QueueType::Graphics);

		// Record compute queue commands
		cmd_buffer = compute->begin_single_time(thread_index);
		executor.execute(cmd_buffer, graph);
		cmd_buffer.release_ownership(*compute, *graphics, target_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		compute->end_single_time(cmd_buffer, nullptr, {}, nullptr, semaphore);

		// Record graphics queue commands to complete ownership transfer
		ph::CommandBuffer gfx_command = graphics->begin_single_time(thread_index);
		gfx_command.acquire_ownership(*compute, *graphics, target_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		graphics->end_single_time(gfx_command, fence, ph::PipelineStage::TopOfPipe, semaphore);
	}

	bool poll() override {
		return ctx.poll_fence(fence);
	}

	void cleanup(uint32_t thread_index) override {
		ph::Queue* compute = ctx.get_queue(ph::QueueType::Compute);
		compute->free_single_time(cmd_buffer, thread_index);
		ctx.end_thread(thread_index);
		ctx.destroy_fence(fence);
		ctx.destroy_semaphore(semaphore);
	}
};

int main() {
	glfwInit();

	ph::AppSettings config;
	config.enable_validation = true;
	config.app_name = "Phobos Test App";
	config.num_threads = 4;
	config.create_headless = false;
	GLFWWindowInterface* wsi = new GLFWWindowInterface("Phobos Test App", 800, 600);
	config.wsi = wsi;
	StdoutLogger* logger = new StdoutLogger;
	config.logger = logger;
	config.gpu_requirements.dedicated = true;
	config.gpu_requirements.min_video_memory = 2u * 1024u * 1024u * 1024u; // 2 GB of video memory
	config.gpu_requirements.requested_queues = { 
		ph::QueueRequest{.dedicated = false, .type = ph::QueueType::Graphics},
		ph::QueueRequest{.dedicated = true, .type = ph::QueueType::Transfer},
		ph::QueueRequest{.dedicated = true, .type = ph::QueueType::Compute}
	};
	config.gpu_requirements.features.samplerAnisotropy = true;
	config.gpu_requirements.features.fillModeNonSolid = true;
	config.gpu_requirements.features.independentBlend = true;
	config.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	// Add descriptor indexing feature
	VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing{};
	descriptor_indexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descriptor_indexing.shaderSampledImageArrayNonUniformIndexing = true;
	descriptor_indexing.runtimeDescriptorArray = true;
	descriptor_indexing.descriptorBindingVariableDescriptorCount = true;
	descriptor_indexing.descriptorBindingPartiallyBound = true;
	config.gpu_requirements.pNext = &descriptor_indexing;
	{
		ph::Context ctx(config);
		ph::Queue& graphics = *ctx.get_queue(ph::QueueType::Graphics);

		ph::RawImage storage_image = ctx.create_image(ph::ImageType::StorageImage, { 2048, 2048 }, VK_FORMAT_R8G8B8A8_UNORM);
		ph::ImageView storage_image_view = ctx.create_image_view(storage_image);

		ctx.name_object(storage_image, "storage image");
		ctx.name_object(storage_image_view, "[view] storage image");

		// Create pipeline for writing to buffer and clearing screen
		ph::ComputePipelineCreateInfo compute_pci =
			ph::ComputePipelineBuilder::create(ctx, "compute_write")
			.set_shader("data/shaders/compute.comp.spv", "main")
			.reflect()
		.get();
		ctx.create_named_pipeline(std::move(compute_pci));

		// Create pipeline for reading from the buffer and displaying the result
		ph::PipelineCreateInfo read_pci =
			ph::PipelineBuilder::create(ctx, "read")
			.add_shader("data/shaders/read.vert.spv", "main", ph::PipelineStage::VertexShader)
			.add_shader("data/shaders/read.frag.spv", "main", ph::PipelineStage::FragmentShader)
			.add_vertex_input(0)
			.add_vertex_attribute(0, 0, VK_FORMAT_R32G32_SFLOAT)
			.add_vertex_attribute(0, 1, VK_FORMAT_R32G32_SFLOAT)
			.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
			.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
			.add_blend_attachment()
			.set_depth_test(false)
			.set_depth_write(false)
			.set_cull_mode(VK_CULL_MODE_NONE)
			.reflect()
			.get();
		ctx.create_named_pipeline(std::move(read_pci));

		TaskScheduler scheduler(ctx.thread_count());
		Task* async_task = new ImageWriteTask(ctx, storage_image_view);
		scheduler.try_run(async_task);

		while (wsi->is_open()) {
			wsi->poll_events();
			ph::InFlightContext ifc = ctx.wait_for_frame();

			scheduler.poll();
			ph::RenderGraph graph{};

			// Buffers for this frame

			ph::TypedBufferSlice<vec4> vbo = ifc.allocate_scratch_vbo<vec4>(sizeof(vertices));
			std::memcpy(vbo.data, vertices, sizeof(vertices));

			ph::Pass read_pass = ph::PassBuilder::create("read")
				.add_attachment(ctx.get_swapchain_attachment_name(), ph::LoadOp::Clear, { .color = {1.0f, 0.0f, 0.0f, 1.0f} })
				.sample_image(storage_image_view, ph::PipelineStage::FragmentShader)
				.execute([&vbo, &ctx, &scheduler, async_task, &storage_image_view](ph::CommandBuffer& cmd_buf) {
					cmd_buf.bind_pipeline("read");
					cmd_buf.auto_viewport_scissor();
					
					// Only do this part if the image is ready. Otherwise log a message saying it's not ready.

					if (scheduler.task_done(async_task)) {
						VkDescriptorSet set = ph::DescriptorBuilder::create(ctx, cmd_buf.get_bound_pipeline())
							.add_sampled_image("image", storage_image_view, ctx.basic_sampler())
							.get();
						cmd_buf.bind_descriptor_set(set);

						cmd_buf.bind_vertex_buffer(0, vbo);
						cmd_buf.draw(6, 1, 0, 0);
					}
					else {
						std::cout << "Image not ready. Skipping draw\n";
					}
				})
				.get();
			graph.add_pass(read_pass);

			graph.build(ctx);

			// Get current command buffer
			ph::CommandBuffer& commands = ifc.command_buffer;
			// Start recording
			commands.begin();
			// Send commands to executor.
			ph::RenderGraphExecutor executor{};
			executor.execute(commands, graph);
			// Stop recording, submit and present
			commands.end();
			ctx.submit_frame_commands(graphics, commands);
			ctx.present(*ctx.get_present_queue());
		}

		ctx.wait_idle();
		ctx.destroy_image_view(storage_image_view);
		ctx.destroy_image(storage_image);
	}
	delete wsi;
	delete logger;

	glfwTerminate();
}
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

int main() {
	glfwInit();

	ph::AppSettings config;
	config.enable_validation = true;
	config.app_name = "Phobos Test App";
	config.num_threads = 16;
	config.create_headless = false;
	GLFWWindowInterface* wsi = new GLFWWindowInterface("Phobos Test App", 800, 600);
	config.wsi = wsi;
	StdoutLogger* logger = new StdoutLogger;
	config.log = logger;
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

		// Create our offscreen attachment
		ctx.create_attachment("offscreen", VkExtent2D{ 500, 500 }, VK_FORMAT_R8G8B8A8_SRGB);

		// Create pipeline for writing to buffer and clearing screen
		ph::PipelineCreateInfo write_pci =
			ph::PipelineBuilder::create(ctx, "write")
			.add_shader("data/shaders/write.vert.spv", "main", ph::PipelineStage::VertexShader)
			.add_vertex_input(0)
			.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
			.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
			.add_vertex_attribute(0, 0, VK_FORMAT_R32G32_SFLOAT)
			.add_vertex_attribute(0, 1, VK_FORMAT_R32G32_SFLOAT)
			.reflect()
		.get();
		ctx.create_named_pipeline(std::move(write_pci));

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

		while (wsi->is_open()) {
			wsi->poll_events();
			ph::InFlightContext ifc = ctx.wait_for_frame();

			ph::RenderGraph graph{};

			// Buffers for this frame

			ph::TypedBufferSlice<vec4> vbo = ifc.allocate_scratch_vbo<vec4>(sizeof(vertices));
			std::memcpy(vbo.data, vertices, sizeof(vertices));

			// We'll write to this buffer in the first pass and read from it in the second one.
			ph::TypedBufferSlice<ShaderSSBO> ssbo = ifc.allocate_scratch_ssbo<ShaderSSBO>(sizeof(ShaderSSBO));

			ph::Pass write_pass =
				ph::PassBuilder::create("write")
				.write_buffer(ssbo, ph::PipelineStage::VertexShader)
				.execute([&vbo, &ssbo, &ctx](ph::CommandBuffer& cmd_buf) {
					cmd_buf.bind_pipeline("write");
					cmd_buf.auto_viewport_scissor();

					VkDescriptorSet set = ph::DescriptorBuilder::create(ctx, cmd_buf.get_bound_pipeline())
						.add_storage_buffer("out_buffer", ssbo)
						.get();
					cmd_buf.bind_descriptor_set(set);

					// We don't support compute yet, so abusing vertex shaders it is
					cmd_buf.bind_vertex_buffer(0, vbo);
					cmd_buf.draw(6, 1, 0, 0);
				})
				.get();
			ph::Pass read_pass =
				ph::PassBuilder::create("read")
				.add_attachment(ctx.get_swapchain_attachment_name(), ph::LoadOp::Clear, { .color = {0.0f, 0.0f, 0.0f, 1.0f} })
				.read_buffer(ssbo, ph::PipelineStage::FragmentShader)
				.execute([&vbo, &ssbo, &ctx](ph::CommandBuffer& cmd_buf) {
					cmd_buf.bind_pipeline("read");
					cmd_buf.auto_viewport_scissor();
					
					VkDescriptorSet set = ph::DescriptorBuilder::create(ctx, cmd_buf.get_bound_pipeline())
						.add_storage_buffer("in_buffer", ssbo)
						.get();
					cmd_buf.bind_descriptor_set(set);

					cmd_buf.bind_vertex_buffer(0, vbo);
					cmd_buf.draw(6, 1, 0, 0);
				})
				.get();
			graph.add_pass(write_pass);
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
	}
	delete wsi;
	delete logger;

	glfwTerminate();
}
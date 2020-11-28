#include <phobos/core/context.hpp>

class GLFWWindowInterface : public ph::WindowInterface {
public:
	std::vector<const char*> window_extension_names() const override {
		return {};
	}
};

int main() {
	ph::AppSettings config;
	config.enable_validation = true;
	config.app_name = "Phobos Test App";
	config.num_threads = 8;
	config.create_headless = false;
	GLFWWindowInterface* wsi = new GLFWWindowInterface;
	config.wsi = wsi;
	config.gpu_requirements.dedicated = true;
	config.gpu_requirements.min_video_memory = 2u * 1024u * 1024u * 1024u; // 2 GB of video memory
	{
		ph::Context ctx(config);
		
	}
	delete wsi;
}
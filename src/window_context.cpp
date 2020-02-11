#include  <phobos/window_context.hpp>

#include <GLFW/glfw3.h>
#include <stdexcept>

namespace ph {

WindowContext::~WindowContext() {

}

WindowContext create_window_context(std::string_view title, size_t width, size_t height, bool fullscreen /* = false*/) {
    WindowContext context;

    // If GLFW was already initialized, this will return true without doing further work
    if (!glfwInit()) {
        throw std::runtime_error("Fatal error: failed to initialize glfw");
    }

    // Initialize for Vulkan (aka no api)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Currently we do not support window resizing
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    context.handle = glfwCreateWindow(width, height, title.data(), monitor, nullptr);

    // Retrieve window size. This may be different from the supplied size if fullscreen was true.
    int w, h;
    glfwGetWindowSize(context.handle, &w, &h);
    context.width = w;
    context.height = h;

    context.title = title;

    return context;
}

bool WindowContext::is_open() const {
    return !glfwWindowShouldClose(handle);
}

void WindowContext::close() {
    glfwSetWindowShouldClose(handle, true);
}

void WindowContext::poll_events() {
    glfwPollEvents();
}


}
#include  <phobos/core/window_context.hpp>

#include <mimas/mimas.h>
#include <mimas/mimas_vk.h>
#include <stdexcept>

namespace ph {

WindowContext::~WindowContext() {

}

WindowContext* create_window_context(std::string_view title, size_t width, size_t height, bool fullscreen /* = false*/) {
    WindowContext* context = new WindowContext;

    // If Mimas was already initialized, this will return true without doing further work
    if (!mimas_init_with_vk()) {
        throw std::runtime_error("Fatal error: failed to initialize mimas");
    }
    

    Mimas_Window_Create_Info window_info;
    window_info.decorated = true;
    window_info.height = height;
    window_info.width = width;
    window_info.title = title.data();
    context->handle = mimas_create_window(window_info);
    mimas_show_window(context->handle);
    // Retrieve window size. This may be different from the supplied size if fullscreen was true.
    int w, h;
    mimas_get_window_content_size(context->handle, &w, &h);
    context->width = w;
    context->height = h;

    context->title = title;

    return context;
}

void destroy_window_context(WindowContext* ctx) {
    delete ctx;
}

bool WindowContext::is_open() const {
    return !mimas_close_requested(handle);
}

void WindowContext::close() {
    mimas_destroy_window(handle);
}

void WindowContext::poll_events() {
    mimas_poll_events();
}


}
#ifndef PHOBOS_WINDOW_CONTEXT_HPP_
#define PHOBOS_WINDOW_CONTEXT_HPP_

#include <phobos/forward.hpp>
#include <string_view>
#include <string>

namespace ph {

struct WindowContext {
    Mimas_Window* handle = nullptr;

    size_t width = 0;
    size_t height = 0;

    std::string title;

    ~WindowContext();

    bool is_open() const;
    void close();

    void poll_events();
};

WindowContext* create_window_context(std::string_view title, size_t width, size_t height, bool fullscreen = false);
void destroy_window_context(WindowContext* ctx);

}

#endif
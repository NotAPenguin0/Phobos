#include <phobos/events/event_dispatcher.hpp>
#include <algorithm>

namespace ph {

void EventDispatcher::fire_event(InstancingBufferResizeEvent const& e) {
    for (auto listener : instancing_buffer_resize_listeners) {
        listener->on_event(e);
    }
}

void EventDispatcher::add_listener(EventListener<InstancingBufferResizeEvent>* listener) {
    instancing_buffer_resize_listeners.push_back(listener);
}

void EventDispatcher::remove_listener(EventListener<InstancingBufferResizeEvent>* listener) {
    instancing_buffer_resize_listeners.erase(
        std::find(instancing_buffer_resize_listeners.begin(), instancing_buffer_resize_listeners.end(), listener));
}

}
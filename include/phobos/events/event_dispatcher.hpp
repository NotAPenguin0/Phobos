#ifndef PHOBOS_EVENT_DISPATCHER_HPP_
#define PHOBOS_EVENT_DISPATCHER_HPP_

#include <phobos/events/events.hpp>
#include <phobos/events/event_listener.hpp>

#include <vector>

namespace ph {

class EventDispatcher {
public:
    // Events
    void fire_event(InstancingBufferResizeEvent const& e);

    // Listeners

    void add_listener(EventListener<InstancingBufferResizeEvent>* listener);

    void remove_listener(EventListener<InstancingBufferResizeEvent>* listener);

private:
    std::vector<EventListener<InstancingBufferResizeEvent>*> instancing_buffer_resize_listeners;
};

}

#endif
#ifndef PHOBOS_EVENT_DISPATCHER_HPP_
#define PHOBOS_EVENT_DISPATCHER_HPP_

#include <phobos/events/events.hpp>
#include <phobos/events/event_listener.hpp>

#include <stl/vector.hpp>

namespace ph {

#define EVENT_DISPATCH_FUNCS(EvtType) \
    void fire_event(EvtType const& e); \
    void add_listener(EventListener<EvtType>* listener); \
    void remove_listener(EventListener<EvtType>* listener)

#define EVENT_LISTENERS(EvtType, EvtName) \
    stl::vector<EventListener<EvtType>*> EvtName##_listeners

class EventDispatcher {
public:

    EVENT_DISPATCH_FUNCS(WindowResizeEvent);
    EVENT_DISPATCH_FUNCS(SwapchainRecreateEvent);

private:
    EVENT_LISTENERS(WindowResizeEvent, window_resize);
    EVENT_LISTENERS(SwapchainRecreateEvent, swapchain_recreate);
};

#undef EVENT_DISPATCH_FUNCS
#undef EVENT_LISTENERS

}

#endif